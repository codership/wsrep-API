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
};

// don't bother with dynamic allocation with a single store
static struct node_store s_store =
{
    .gtid       = {{{0,}}, WSREP_SEQNO_UNDEFINED} /*WSREP_GTID_UNDEFINED*/,
    .gtid_mtx   = PTHREAD_MUTEX_INITIALIZER,
    .trx_id     = 0,
    .trx_id_mtx = PTHREAD_MUTEX_INITIALIZER
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

int
node_store_init(struct node_store*  const store,
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

    store->gtid = *gtid;
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
