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

#include <arpa/inet.h> // htonl()
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stddef.h>   // ptrdiff_t
#include <stdint.h>   // uintptr_t
#include <stdlib.h>   // abort()
#include <string.h>   // memset()

typedef wsrep_uuid_t member_t;
typedef uint32_t     record_t;

struct node_store
{
    wsrep_gtid_t    gtid;
    pthread_mutex_t gtid_mtx;
    wsrep_trx_id_t  trx_id;
    pthread_mutex_t trx_id_mtx;
    char*           snapshot;
    uint32_t        members_num;
    member_t*       members;
    uint32_t        records_num;
    record_t*       records;
};

node_store_t*
node_store_open(const struct node_options* const opts)
{
    struct node_store* ret = calloc(1, sizeof(struct node_store));

    if (ret)
    {
        ret->records = malloc((size_t)opts->records * sizeof(*ret->records));

        if (ret->records)
        {
            ret->gtid = WSREP_GTID_UNDEFINED;
            pthread_mutex_init(&ret->gtid_mtx, NULL);
            pthread_mutex_init(&ret->trx_id_mtx, NULL);
            ret->records_num = (uint32_t)opts->records;

            uint32_t i;
            for (i = 0; i < ret->records_num; i++)
            {
                ret->records[i] = i;
            }
        }
        else
        {
            free(ret);
            ret = NULL;
        }
    }

    return ret;
}

void
node_store_close(struct node_store* store)
{
    assert(store);
    assert(store->records);
    pthread_mutex_destroy(&store->gtid_mtx);
    pthread_mutex_destroy(&store->trx_id_mtx);
    free(store->records);
    free(store->members);
    free(store);
}

/**
 * deserializes membership from snapshot */
static int
store_new_members(const char* ptr, const char* const endptr,
                  uint32_t* const num, member_t** const memb)
{
    memcpy(num, ptr, sizeof(*num));
    *num = ntohl(*num);

    if (*num < 2)
    {
        NODE_ERROR("Bogus number of members %u", *num);
        return -1;
    }

    int ret = (int)sizeof(*num);
    ptr += ret;

    size_t const msize = sizeof(member_t) * *num;
    if ((endptr - ptr) < (ptrdiff_t)msize)
    {
        NODE_ERROR("State snapshot does not contain all membership: "
                   "%zd < %zu", endptr - ptr, msize);
        return -1;
    }

    *memb = calloc(*num, sizeof(member_t));
    if (!*memb)
    {
        NODE_ERROR("Could not allocate new membership");
        return -ENOMEM;
    }

    memcpy(*memb, ptr, msize);

    return ret + (int)msize;
}

/**
 * deserializes records from snapshot */
static int
store_new_records(const char* ptr, const char* const endptr,
                  uint32_t* const num, record_t** const rec)
{
    memcpy(num, ptr, sizeof(*num));
    *num = ntohl(*num);

    int ret = (int)sizeof(*num);
    if (!*num)
    {
        *rec = NULL;
        return ret;
    }

    ptr += ret;
    size_t const rsize = sizeof(record_t) * *num;
    if ((endptr - ptr) < (ptrdiff_t)rsize)
    {
        NODE_ERROR("State snapshot does not contain all records: "
                   "%zu < %zu", endptr - ptr, rsize);
        return -1;
    }

    *rec = calloc(*num, sizeof(record_t));
    if (!*rec)
    {
        NODE_ERROR("Could not allocate new records");
        return -ENOMEM;
    }

    memcpy(*rec, ptr, rsize);

    uint32_t i;
    for (i = 0; i < *num; i++)
    {
        /* Not doing it derectly from ptr due to potential alignment issues.
         * Bulk memcpy() above is way more efficient */
        (*rec)[i] = ntohl((*rec)[i]);
    }

    return ret + (int)rsize;
}

int
node_store_init_state(struct node_store*  const store,
                      const void*         const state,
                      size_t              const state_len)
{
    /* First, desewrialize and prepare new state */
    if (state_len <= sizeof(wsrep_uuid_t)*2 /* at least two members */ +
        WSREP_UUID_STR_LEN + 1 /* : */ + 1 /* \0 */)
    {
        NODE_ERROR("State snapshot too short: %zu", state_len);
        return -1;
    }

    wsrep_gtid_t state_gtid;
    int ret;
    ret = wsrep_gtid_scan(state, state_len, &state_gtid);
    if (ret < 0)
    {
        char state_str[WSREP_GTID_STR_LEN + 1] = { 0, };
        memcpy(state_str, state, sizeof(state_str) - 1);
        NODE_ERROR("Could not find valid GTID in the received data: %s",
                    state_str);
        return -1;
    }

    ret++; /* \0 */
    if ((state_len - (size_t)ret) < sizeof(uint32_t))
    {
        NODE_ERROR("State snapshot does not contain number of members");
        return -1;
    }

    const char* ptr = ((char*)state);
    const char* const endptr = ptr + state_len;
    ptr += ret;

    uint32_t m_num;
    member_t* new_members;
    ret = store_new_members(ptr, endptr, &m_num, &new_members);
    if (ret < 0)
    {
        return ret;
    }
    ptr += ret;

    uint32_t r_num;
    record_t* new_records;
    ret = store_new_records(ptr, endptr, &r_num, &new_records);
    if (ret < 0)
    {
        free(new_members);
        return ret;
    }
    ptr += ret;

    ret = pthread_mutex_lock(&store->gtid_mtx);
    if (ret)
    {
        NODE_FATAL("Failed to lock store GTID");
        abort();
    }

    /* just a sanity check */
    if (0 == wsrep_uuid_compare(&state_gtid.uuid, &store->gtid.uuid) &&
        state_gtid.seqno < store->gtid.seqno)
    {
        NODE_ERROR("Received snapshot that is in the past: my seqno %lld,"
                   " received seqno: %lld",
                   (long long)store->gtid.seqno, (long long)state_gtid.seqno);
        free(new_members);
        free(new_records);
        ret = -1;
    }
    else
    {
        free(store->members);
        store->members_num = m_num;
        store->members     = new_members;
        free(store->records);
        store->records_num = r_num;
        store->records     = new_records;
        store->gtid        = state_gtid;
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
    int ret = pthread_mutex_lock(&store->gtid_mtx);
    if (ret)
    {
        NODE_FATAL("Failed to lock store GTID");
        abort();
    }

    char*    ptr;
    uint32_t num;

    if (!store->snapshot)
    {
        size_t const memb_len = store->members_num * sizeof(member_t);
        size_t const rec_len  = store->records_num * sizeof(record_t);
        size_t const buf_len  = WSREP_GTID_STR_LEN + 1
            + sizeof(uint32_t) + memb_len
            + sizeof(uint32_t) + rec_len;

        store->snapshot = malloc(buf_len);

        if (store->snapshot)
        {
            ptr = store->snapshot;

            /* state GTID */
            ret = wsrep_gtid_print(&store->gtid, ptr, buf_len);
            if (ret > 0)
            {
                NODE_INFO("");
                assert((size_t)ret < buf_len);

                ptr[ret] = '\0';
                ret++;
                ptr += ret;
                assert((size_t)ret < buf_len);

                /* membership */
                num = htonl(store->members_num);
                memcpy(ptr, &num, sizeof(num));
                ptr += sizeof(num);
                ret += (int)sizeof(num);
                assert((size_t)ret + memb_len < buf_len);
                memcpy(ptr, store->members, memb_len);
                ptr += memb_len;
                ret += (int)memb_len;
                assert((size_t)ret + sizeof(uint32_t) <= buf_len);

                /* records */
                num = htonl(store->records_num);
                memcpy(ptr, &num, sizeof(num));
                ptr += sizeof(num);
                ret += (int)sizeof(num);
                assert((size_t)ret + rec_len < buf_len);
                memcpy(ptr, store->records, rec_len);
                ret += (int)rec_len;
                assert((size_t)ret <= buf_len);
            }
            else
            {
                NODE_ERROR("Failed to record GTID: %d (%s)", ret,strerror(-ret));
                free(store->snapshot);
                store->snapshot = 0;
            }
        }
        else
        {
            NODE_ERROR("Failed to allocate snapshot buffer of size %zu",buf_len);
            ret = -ENOMEM;
        }
    }
    else
    {
        assert(0); /* provider should prevent such situation */
        ret = -EAGAIN;
    }

    pthread_mutex_unlock(&store->gtid_mtx);

    if (ret > 0)
    {
        uint32_t i;
        record_t* r = (record_t*)ptr; /* poorly aligned */

        num = ntohl(num); /* back from network order */
        for (i = 0; i < num; i++)
        {
            record_t wa; /* well aligned */
            memcpy(&wa, &r[i], sizeof(wa));
            wa = htonl(wa);
            memcpy(&r[i], &wa, sizeof(wa));
        }
        NODE_INFO("\n\nPrepared snapshot of %u records\n\n", num);
        *state     = store->snapshot;
        *state_len = (size_t)ret;
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
node_store_update_membership(struct node_store*       const store,
                             const wsrep_view_info_t* const v)
{
    assert(store);
    assert(WSREP_VIEW_PRIMARY == v->status);
        assert(v->memb_num > 0);

    int ret;
    ret = pthread_mutex_lock(&store->gtid_mtx);
    if (ret)
    {
        NODE_FATAL("Failed to lock store GTID");
        abort();
    }

    bool const continuation = v->state_id.seqno == store->gtid.seqno + 1 &&
        0 == wsrep_uuid_compare(&v->state_id.uuid, &store->gtid.uuid);

    bool const initialization = WSREP_SEQNO_UNDEFINED == store->gtid.seqno &&
        0 == wsrep_uuid_compare(&WSREP_UUID_UNDEFINED, &store->gtid.uuid);

    if (!(continuation || initialization))
    {
        char store_str[WSREP_GTID_STR_LEN + 1] = { 0, };
        wsrep_gtid_print(&store->gtid, store_str, sizeof(store_str));
        char view_str[WSREP_GTID_STR_LEN + 1] = { 0, };
        wsrep_gtid_print(&v->state_id, view_str, sizeof(view_str));

        NODE_FATAL("Attempt to initialize store GTID from incompatible view:\n"
                   "\tstore: %s\n"
                   "\tview:  %s",
                   store_str, view_str);
        abort();
    }

    wsrep_uuid_t* const new_members = calloc(sizeof(wsrep_uuid_t),
                                             (size_t)v->memb_num);
    if (!new_members)
    {
        NODE_FATAL("Could not allocate new members array");
        abort();
    }

    int i;
    for (i = 0; i < v->memb_num; i++)
    {
        new_members[i] = v->members[i].id;
    }

    /* REPLICATION: at this point we should compare old and new memberships and
     *              rollback all streaming transactions from the partitioned
     *              members, if any. But we don't support it in this program yet.
     */

    free(store->members);

    store->members     = new_members;
    store->members_num = (uint32_t)v->memb_num;
    store->gtid        = v->state_id;

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


/* transaction context */
struct store_trx_ctx
{
    uint32_t to;
    record_t val;
};

union store_trx_ctx_aligned { struct store_trx_ctx a; wsrep_buf_t b; };

int
node_store_execute(node_store_t*   const store,
                   wsrep_trx_id_t* const trx_id,
                   wsrep_key_t*    const key,
                   wsrep_buf_t*    const ws)
{
    assert(store);

    uint32_t to, from;
    record_t val;
    int ret = pthread_mutex_lock(&store->gtid_mtx);
    if (ret)
    {
        NODE_FATAL("Failed to lock store GTID");
        abort();
    }

    /* Transaction: copy value from one random record to another
     * and increment it by 1 */
    to   = (uint32_t)rand() % store->records_num;
    from = (uint32_t)rand() % store->records_num;
    val  = store->records[from] + 1;

    pthread_mutex_unlock(&store->gtid_mtx);

    static size_t const key_parts_offset = sizeof(union store_trx_ctx_aligned);
    size_t const key_vals_offset = key_parts_offset +
        2 * sizeof(wsrep_buf_t); /* array of 2 key part structs */
    size_t const ws_offset = key_vals_offset + 2 * sizeof(to); /* 2 key values */
    size_t const ws_len = ws->len > sizeof(to) + sizeof(val) ?
        ws->len : sizeof(to) + sizeof(val);
    size_t const alloc_size = ws_offset + ws_len;
    char*  const buf = malloc(alloc_size);

    /* store allocated memory address directly in trx_id to avoid implementing
     * hashtable between trx_id and allocated resource for now.
     * Thus we can free it right away in commit/rollback */
    assert(sizeof(*trx_id) >= sizeof(buf));
    *trx_id = (uintptr_t)buf;
    if (!buf)
    {
        return -ENOMEM;
    }

    /* record "prepared" transaction in transaction context for use in commit */
    struct store_trx_ctx* trx = (struct store_trx_ctx*)buf;
    trx->to  = to;
    trx->val = val;

    to   = htonl(to);
    from = htonl(from);
    val  = htonl(val);

    /* REPLICATION: creating key parts */
    wsrep_buf_t* key_parts = (wsrep_buf_t*)(buf + key_parts_offset);
    uint32_t*    key_vals  = (uint32_t*)   (buf + key_vals_offset);

    memcpy(&key_vals[0], &from, sizeof(from));
    key_parts[0].ptr = &key_vals[0];
    key_parts[0].len = sizeof(from);

    memcpy(&key_vals[1], &to, sizeof(to));
    key_parts[1].ptr = &key_vals[1];
    key_parts[1].len = sizeof(to);

    /* REPLICATION: recording key parts in a key struct
     * NOTE: depending on data access granularity some applications may require
     *       multipart keys, e.g. <schema>:<table>:<row> in a SQL database.
     *       Single part keys match hashtables and key-value stores.
     *       Here we have two key parts representing two different single-part
     *       keys which reference two different records. For convenience however
     *       we'll return them as two parts in a single wsrep_key struct which
     *       here serves only as a container. */
    key->key_parts     = key_parts;
    key->key_parts_num = 2;

    /* REPLICATION: filling the actual writeset */
    char* const ws_ptr = buf + ws_offset;
    memcpy(ws_ptr, &to, sizeof(to));
    memcpy(ws_ptr + sizeof(to), &val, sizeof(val));
    ws->ptr = ws_ptr;
    ws->len = ws_len;

    return 0;
}

int
node_store_apply(node_store_t*      const store,
                 wsrep_trx_id_t*    const trx_id,
                 const wsrep_buf_t* const ws)
{
    assert(store);
    (void)store;

    struct store_trx_ctx* const trx = malloc(sizeof(struct store_trx_ctx));
    *trx_id = (uintptr_t)trx;
    if (!trx)
    {
        return -ENOMEM;
    }

    /* prepare values for commit */
    uint32_t to;
    record_t val;

    assert(ws->len >= sizeof(to) + sizeof(val));

    memcpy(&to, ws->ptr, sizeof(to));
    memcpy(&val, (char*)ws->ptr + sizeof(to), sizeof(val));

    trx->to  = ntohl(to);
    trx->val = ntohl(val);

    assert(trx->to <= store->records_num);

    return 0;
}

static uint32_t const store_fnv32_seed  = 2166136261;

static inline uint32_t
store_fnv32a(const void* buf, size_t const len, uint32_t seed)
{
    static uint32_t const fnv32_prime = 16777619;
    const uint8_t* bp = (const uint8_t*)buf;
    const uint8_t* const be = bp + len;

    while (bp < be)
    {
        seed ^= *bp++;
        seed *= fnv32_prime;
    }

    return seed;
}


static void
store_checksum_state(node_store_t* store)
{
    uint32_t res = store_fnv32_seed;
    uint32_t i;

    for (i = 0; i < store->members_num; i++)
    {
        res = store_fnv32a(&store->members[i], sizeof(*store->members), res);
    }

    for (i = 0; i < store->records_num; i++)
    {
        uint32_t val = htonl(store->records[i]);
        res = store_fnv32a(&val, sizeof(val), res);
    }

    res = store_fnv32a(&store->gtid.uuid, sizeof(store->gtid.uuid), res);

    wsrep_seqno_t s = store->gtid.seqno;
    for (i = 0; i < sizeof(s); i++)
    {
        char a = (char)(s & 0xff);
        res = store_fnv32a(&a, sizeof(a), res);
        s >>= 8;
    }

    NODE_INFO("\n\n\tSeqno: %lld; state hash: %#010x\n",
              (long long)store->gtid.seqno, res);
}

static inline void
store_update_gtid(node_store_t* const store, const wsrep_gtid_t* ws_gtid)
{
    assert(0 == wsrep_uuid_compare(&store->gtid.uuid, &ws_gtid->uuid));

    store->gtid.seqno++;

    if (store->gtid.seqno != ws_gtid->seqno)
    {
        NODE_FATAL("Out of order commit: expected %lld, got %lld",
                   store->gtid.seqno, ws_gtid->seqno);
        abort();
    }

    static wsrep_seqno_t const period = 0x1fffff; /* ~2M */
    if (0 == (store->gtid.seqno & period))
    {
        store_checksum_state(store);
    }
}

void
node_store_commit(node_store_t*       const store,
                  wsrep_trx_id_t      const trx_id,
                  const wsrep_gtid_t* const ws_gtid)
{
    assert(store);

    struct store_trx_ctx* const trx = (struct store_trx_ctx*)trx_id;
    assert(trx);

    int ret;
    ret = pthread_mutex_lock(&store->gtid_mtx);
    if (ret)
    {
        NODE_FATAL("Failed to lock store GTID");
        abort();
    }

    store_update_gtid(store, ws_gtid);

    store->records[trx->to] = trx->val;

    pthread_mutex_unlock(&store->gtid_mtx);

    free(trx);
}

void
node_store_rollback(node_store_t*  store,
                    wsrep_trx_id_t trx_id)
{
    assert(store);
    (void)store;
    assert(trx_id);

    free((void*)trx_id); /* all resources allocated for trx are here */
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

    store_update_gtid(store, ws_gtid);

    pthread_mutex_unlock(&store->gtid_mtx);
}
