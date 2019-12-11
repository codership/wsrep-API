/* Copyright (c) 2019, Codership Oy. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "sst.h"

#include "ctx.h"
#include "log.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h> // abort()
#include <string.h> // strdup()
#include <unistd.h> // usleep()

static const char* const _sst_request = "fake SST";

/**
 * Helper: creates detached thread */
static int
_sst_create_thread(void* (*thread_routine) (void*),
                   void* const thread_arg)
{
    pthread_t thr;
    pthread_attr_t attr;
    int ret = pthread_attr_init(&attr);
    ret = ret ? ret : pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    ret = ret ? ret : pthread_create(&thr, &attr, thread_routine, thread_arg);
    return ret;
}

/**
 * Helper: creates detached thread and waits for it to call
 *         _sst_sync_with_parent() */
static void
_sst_create_and_sync(const char*      const role,
                     pthread_mutex_t* const mtx,
                     pthread_cond_t*  const cond,
                     void* (*thread_routine) (void*),
                     void*            const thread_arg)
{
    int ret = pthread_mutex_lock(mtx);
    if (ret)
    {
        NODE_FATAL("Failed to lock %s mutex: %d (%s)", role, ret, strerror(ret));
        abort();
    }

    ret = _sst_create_thread(thread_routine, thread_arg);
    if (ret)
    {
        NODE_FATAL("Failed to create detached %s thread: %d (%s)",
                   role, ret, strerror(ret));
        abort();
    }

    ret = pthread_cond_wait(cond, mtx);
    if (ret)
    {
        NODE_FATAL("Failed to synchronize with %s thread: %d (%s)",
                   role, ret, strerror(ret));
        abort();
    }

    pthread_mutex_unlock(mtx);
}

/**
 * Helper: syncs with parent thread and allows it to continue and return
 *         asynchronously */
static void
_sst_sync_with_parent(const char*      role,
                      pthread_mutex_t* mtx,
                      pthread_cond_t*  cond)
{
    int ret = pthread_mutex_lock(mtx);
    if (ret)
    {
        NODE_FATAL("Failed to lock %s mutex: %d (%s)", role, ret, strerror(ret));
        abort();
    }

    NODE_INFO("Initialized %s thread", role);

    pthread_cond_signal(cond);
    pthread_mutex_unlock(mtx);
}

static pthread_mutex_t _joiner_mtx  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  _joiner_cond = PTHREAD_COND_INITIALIZER;

/**
 * waits for SST completion and signals the provider to continue */
static void*
_joiner_thread(void* const app_ctx)
{
    assert(app_ctx);

    struct node_ctx* const node  = app_ctx;

    /* this allows parent callback to return */
    _sst_sync_with_parent("JOINER", &_joiner_mtx, &_joiner_cond);

    /* REPLICATION: once SST is received, the GTID which it brought is recorded
     *              the store, Get it from there to tell provider what we have */
    wsrep_gtid_t sst_gtid;
    node_store_gtid(node->store, &sst_gtid);

    /* Had we sent a real SST request, we'd be waiting for it from some donor
     * node. Since we lack that synchronizatoin opportunity, we'd do a dirty
     * trick and "poll" provider for success (reaching JOINER state). */
    wsrep_status_t sst_ret;
    wsrep_t* const wsrep = node_wsrep_provider(node->wsrep);
    do
    {
        usleep(10000); // 10ms
        /* REPLICATION: tell provider that SST is received */
        sst_ret = wsrep->sst_received(wsrep, &sst_gtid, NULL, 0);
    }
    while (WSREP_CONN_FAIL == sst_ret);

    if (sst_ret)
    {
        NODE_FATAL("Failed to report SST received: %d", sst_ret);
        abort();
    }

    return NULL;
}

// stub
enum wsrep_cb_status
node_sst_request_cb (void*   const app_ctx,
                     void**  const sst_req,
                     size_t* const sst_req_len)
{
    assert(app_ctx);

    /* REPLICATION: 1. prepare the node to receive SST */

    char* const sst_str = strdup(_sst_request);
    if (sst_str)
    {
        *sst_req = sst_str;
        *sst_req_len = strlen(sst_str) + 1;

        /* Temporary hack: use the GTID of the cluster to which we connected
         * to initialize store GTID. In reality store GTID should be initialized
         * by real SST. */
        struct node_ctx* const node = app_ctx;
        wsrep_gtid_t state_id;
        node_wsrep_connected_gtid(node->wsrep, &state_id);
        node_store_init(node->store, &state_id);
    }
    else
    {
        *sst_req = NULL;
        *sst_req_len = 0;
        return WSREP_CB_FAILURE;
    }

    /* REPLICATION 2. start the "joiner" thread that will wait for SST and
     *                report its success to provider, and syncronize with it. */
    _sst_create_and_sync("JOINER", &_joiner_mtx, &_joiner_cond,
                         _joiner_thread, app_ctx);

    /* 3. return SST request to provider */

    return WSREP_CB_SUCCESS;
}

static pthread_mutex_t _donor_mtx  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  _donor_cond = PTHREAD_COND_INITIALIZER;

struct _donor_ctx
{
    wsrep_gtid_t     state;
    struct node_ctx* node;
    wsrep_bool_t     bypass;
};

/**
 * donates SST and signals provider that it is done. */
static void*
_donor_thread(void* const args)
{
    struct _donor_ctx const ctx = *(struct _donor_ctx*)args;

    /* having saved the context, we can allow parent callback to return */
    _sst_sync_with_parent("DONOR", &_donor_mtx, &_donor_cond);

    /* REPLICATION: signal provider that we are done */
    wsrep_t* const wsrep = node_wsrep_provider(ctx.node->wsrep);
    wsrep->sst_sent(wsrep, &ctx.state, 0);

    return NULL;
}

// stub
enum wsrep_cb_status
node_sst_donate_cb (void*               const app_ctx,
                    void*               const recv_ctx,
                    const wsrep_buf_t*  const str_msg,
                    const wsrep_gtid_t* const state_id,
                    const wsrep_buf_t*  const state,
                    wsrep_bool_t        const bypass)
{
    (void)recv_ctx;
    (void)state;

    if (strncmp(_sst_request, str_msg->ptr, str_msg->len))
    {
        NODE_ERROR("Can't serve non-trivial SST: not implemented");
        return WSREP_CB_FAILURE;
    }

    struct _donor_ctx ctx =
    {
        .node   = app_ctx,
        .state  = *state_id,
        .bypass = bypass
    };

    _sst_create_and_sync("DONOR", &_donor_mtx, &_donor_cond,
                         _donor_thread, &ctx);

    return WSREP_CB_SUCCESS;
}
