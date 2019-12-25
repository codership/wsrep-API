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

#include "store.h"

#include "log.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>   // uintptr_t
#include <stdlib.h>   // abort()
#include <string.h>   // memset()

struct node_store
{
    wsrep_gtid_t    gtid;
    pthread_mutex_t gtid_mtx;
    wsrep_trx_id_t  trx_id;
    pthread_mutex_t trx_id_mtx;
    char*           snapshot;
};

// don't bother with dynamic allocation with a single store
static struct node_store s_store =
{
    .gtid       = {{{0,}}, WSREP_SEQNO_UNDEFINED} /*WSREP_GTID_UNDEFINED*/,
    .gtid_mtx   = PTHREAD_MUTEX_INITIALIZER,
    .trx_id     = 0,
    .trx_id_mtx = PTHREAD_MUTEX_INITIALIZER,
    .snapshot   = NULL
};

static struct node_store* store_ptr = &s_store;

// stub
node_store_t*
node_store_open(const struct node_options* const opts)
{
    (void)opts;
    node_store_t* ret = store_ptr;
    store_ptr = NULL;
    return ret;
}

// stub
void
node_store_close(struct node_store* store)
{
    assert(store);
    (void)store;
}

#define STORE_STATE_MIN_LEN (WSREP_GTID_STR_LEN + 1)

int
node_store_init_state(struct node_store*  const store,
                      const void*         const state,
                      size_t              const state_len)
{
    /* First, decipher state */
    if (state_len <= WSREP_UUID_STR_LEN + 1 /* : */ + 1 /* \0 */)
    {
        NODE_ERROR("State snapshot too short: %zu", state_len);
        return -1;
    }

    wsrep_gtid_t state_gtid;
    int ret;
    ret = wsrep_gtid_scan(state, state_len, &state_gtid);
    if (ret < 0)
    {
        char state_str[STORE_STATE_MIN_LEN] = { 0, };
        memcpy(state_str, state, sizeof(state_str) - 1);
        NODE_ERROR("Could not find valid GTID in the received data: %s",
                    state_str);
        return -1;
    }

    ret = pthread_mutex_lock(&store->gtid_mtx);
    if (ret)
    {
        NODE_FATAL("Failed to lock store GTID");
        abort();
    }

    /* just some sanity check */
    if (0 == wsrep_uuid_compare(&state_gtid.uuid, &store->gtid.uuid) &&
        state_gtid.seqno < store->gtid.seqno)
    {
        NODE_ERROR("Received snapshot that is in the past: my seqno %lld,"
                   " received seqno: %lld",
                   (long long)store->gtid.seqno, (long long)state_gtid.seqno);
        ret = -1;
    }
    else
    {
        store->gtid = state_gtid;
        ret = 0;
    }

    pthread_mutex_unlock(&store->gtid_mtx);

    return ret;
}

int
node_store_acquire_state(node_store_t* const store,
                         const void**  const state,
                         size_t*       const state_len)
{
    char gtid_str[STORE_STATE_MIN_LEN] = { 0, };

    int ret = pthread_mutex_lock(&store->gtid_mtx);
    if (ret)
    {
        NODE_FATAL("Failed to lock store GTID");
        abort();
    }

    if (!store->snapshot)
    {
        ret = wsrep_gtid_print(&store->gtid, gtid_str, sizeof(gtid_str));
        if (ret > 0) store->snapshot = strdup(gtid_str);
        if (!store->snapshot) ret = -ENOMEM;
    }
    else
    {
        ret = -EAGAIN;
    }

    pthread_mutex_unlock(&store->gtid_mtx);

    if (ret > 0)
    {
        assert(strlen(store->snapshot) == (size_t)ret);
        *state     = store->snapshot;
        *state_len = (size_t)ret + 1 /* \0 */;
        ret        = 0;
    }

    return ret;
}

void
node_store_release_state(node_store_t* const store)
{
    int ret = pthread_mutex_lock(&store->gtid_mtx);
    if (ret)
    {
        NODE_FATAL("Failed to lock store GTID");
        abort();
    }

    assert(store->snapshot);
    free(store->snapshot);
    store->snapshot = 0;

    pthread_mutex_unlock(&store->gtid_mtx);
}

int
node_store_init_gtid(struct node_store*  const store,
                     const wsrep_gtid_t* const gtid)
{
    assert(store);

    int ret;
    ret = pthread_mutex_lock(&store->gtid_mtx);
    if (ret)
    {
        NODE_FATAL("Failed to lock store GTID");
        abort();
    }

    if (0 == wsrep_uuid_compare(&WSREP_UUID_UNDEFINED, &store->gtid.uuid) &&
        WSREP_SEQNO_UNDEFINED == store->gtid.seqno)
    {
        store->gtid = *gtid;
    }
    else
    {
        char gtid_str[WSREP_GTID_STR_LEN + 1] = { 0, };
        wsrep_gtid_print(&store->gtid, gtid_str, sizeof(gtid_str));
        NODE_FATAL("Attempt to initialize state GTID which is already "
                   "initialized: %s", gtid_str);
        abort();
    }

    pthread_mutex_unlock(&store->gtid_mtx);

    return ret;
}

void
node_store_gtid(struct node_store* const store,
                wsrep_gtid_t*      const gtid)
{
    assert(store);

    int ret;
    ret = pthread_mutex_lock(&store->gtid_mtx);
    if (ret)
    {
        NODE_FATAL("Failed to lock store GTID");
        abort();
    }

    *gtid = store->gtid;
    pthread_mutex_unlock(&store->gtid_mtx);
}

// stub
int
node_store_execute(node_store_t*   const store,
                   wsrep_trx_id_t* const trx_id,
                   wsrep_key_t*    const key,
                   wsrep_buf_t*    const ws)
{
    assert(store);
    (void)store;

    size_t const key_len = sizeof(uint64_t);
    size_t const alloc_size = sizeof(wsrep_buf_t) + key_len + ws->len;
    char* buf = malloc(alloc_size);

    /* store allocated memory address directly in trx_id to avoid implementing
     * hashtable between trx_id and allocated resource for now.
     * Thus we can free it right away in commit/rollback */
    assert(sizeof(*trx_id) >= sizeof(buf));
    *trx_id = (uintptr_t)buf;
    if (!buf)
    {
        return ENOMEM;
    }

    /* REPLICATION: creating a key part */
    wsrep_buf_t* key_part = (wsrep_buf_t*)buf; // malloc'd, so well aligned
    void* const key_ptr = buf + sizeof(wsrep_buf_t); // to avoid const cast
    memcpy(key_ptr, trx_id, key_len); // this be sufficiently random key
    key_part->ptr  = key_ptr;
    key_part->len  = key_len;

    /* REPLICATION: recording a single key part in a key struct
     * NOTE: depending on data access granularity some applications may require
     *       multipart keys, e.g. <schema>:<table>:<row> in a SQL database.
     *       Single part keys match hashtables and key-value stores */
    key->key_parts     = key_part;
    key->key_parts_num = 1;

    /* filling the actual writeset */
    void* const ws_ptr = buf + sizeof(wsrep_buf_t) + key_len; // avoid const cast
    memset(ws_ptr, (int)*trx_id, ws->len);  // just initialize with something
    ws->ptr = ws_ptr;

    return 0;
}

// stub
int
node_store_apply(node_store_t*      const store,
                 wsrep_trx_id_t*    const trx_id,
                 const wsrep_buf_t* const ws)
{
    assert(store);
    (void)store;

    *trx_id = (uintptr_t)NULL; // not allocating any resources, no ID
    (void)ws;
    return 0;
}

// stub
void
node_store_commit(node_store_t*       const store,
                  wsrep_trx_id_t      const trx_id,
                  const wsrep_gtid_t* const ws_gtid)
{
    assert(store);

    node_store_update_gtid(store, ws_gtid);
    if (trx_id > 0) free((void*)trx_id);
}

// stub
void
node_store_rollback(node_store_t*  store,
                    wsrep_trx_id_t trx_id)
{
    assert(store);
    (void)store;

    if (trx_id > 0) free((void*)trx_id);
}

void
node_store_update_gtid(node_store_t*       store,
                       const wsrep_gtid_t* ws_gtid)
{
    assert(store);

    int ret;
    ret = pthread_mutex_lock(&store->gtid_mtx);
    if (ret)
    {
        NODE_FATAL("Failed to lock store GTID");
        abort();
    }

    assert(0 == wsrep_uuid_compare(&store->gtid.uuid, &ws_gtid->uuid));

    store->gtid.seqno++;
    if (store->gtid.seqno != ws_gtid->seqno)
    {
        NODE_FATAL("Out of order commit: expected %lld, got %lld",
                   store->gtid.seqno, ws_gtid->seqno);
        abort();
    }

    pthread_mutex_unlock(&store->gtid_mtx);
}
