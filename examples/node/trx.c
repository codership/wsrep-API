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

#include "trx.h"

#include <stdlib.h> // malloc()
#include <errno.h>  // ENOMEM, etc.
#include <stdbool.h>
#include <assert.h>

wsrep_status_t
node_trx_execute(node_store_t* const   store,
                 wsrep_t* const        wsrep,
                 wsrep_conn_id_t const conn_id,
                 size_t const          ws_size)
{
    wsrep_trx_id_t trx_id;
    wsrep_key_t    ws_key;
    wsrep_buf_t    ws = { NULL, ws_size };

    /* prepare simple transaction and return the key and data buffer */
    node_store_execute(store, &trx_id, &ws_key, &ws);
    wsrep_status_t cert = WSREP_FATAL; // for cleanup

    /* REPLICATION: get wsrep handle for transaction */
    wsrep_ws_handle_t ws_handle = { trx_id, NULL };

    /* REPLICATION: create a writeset for trx */
    wsrep_status_t ret;
    ret = wsrep->append_key(wsrep, &ws_handle,
                            &ws_key,
                            1,    /* single key */
                            WSREP_KEY_EXCLUSIVE,
                            false /* no need to make a copy */);
    if (ret) goto cleanup;
    ret = wsrep->append_data(wsrep, &ws_handle,
                             &ws, 1, WSREP_DATA_ORDERED, false);
    if (ret) goto cleanup;

    /* REPLICATION: (replicate and) certify the writeset with the cluster */
    static unsigned int const ws_flags =
        WSREP_FLAG_TRX_START | WSREP_FLAG_TRX_END; // atomic trx
    wsrep_trx_meta_t ws_meta;
    cert = wsrep->certify(wsrep, conn_id, &ws_handle, ws_flags, &ws_meta);

    if (WSREP_BF_ABORT == cert)
    {
        /* REPLICATON: transaction was signaled to abort due to multi-master
         *             conflict. It must rollback immediately: it blocks
         *             transaction that was ordered earlier and will never
         *             be able to enter commit order. */
        node_store_rollback(store, trx_id);
    }

    /* REPLICATION: writeset was totally ordered, need to enter commit order */
    if (ws_meta.gtid.seqno > 0)
    {
        ret = wsrep->commit_order_enter(wsrep, &ws_handle, &ws_meta);
        if (ret) goto cleanup;

        /* REPLICATION: inside commit monitor
         * Note: we commit transaction only if certification succeded */
        if (WSREP_OK == cert) node_store_commit(store, trx_id, &ws_meta.gtid);
        else                  node_store_update_gtid(store, &ws_meta.gtid);

        ret = wsrep->commit_order_leave(wsrep, &ws_handle, &ws_meta, NULL);
        if (ret) goto cleanup;
    }
    else
    {
        assert(cert);
    }

cleanup:
    /* REPLICATION: if wsrep->certify() returned anything else but WSREP_OK
     *              transaction must roll back. BF aborted trx already did it. */
    if (cert && WSREP_BF_ABORT != cert) node_store_rollback(store, trx_id);

    /* NOTE: this application follows the approach that resources must be freed
     *       at the same level where they were allocated, so it is assumed that
     *       ws_key and ws were deallocated in either commit or rollback calls.*/

    /* REPLICATION: release provider resources associated with the trx */
    wsrep->release(wsrep, &ws_handle);

    return ret ? ret : cert;
}

wsrep_status_t
node_trx_apply(node_store_t*            const store,
               wsrep_t*                 const wsrep,
               const wsrep_ws_handle_t* const ws_handle,
               const wsrep_trx_meta_t*  const ws_meta,
               const wsrep_buf_t*       const ws)
{
    /* no business being here if event was not ordered */
    assert(ws_meta->gtid.seqno > 0);

    wsrep_trx_id_t trx_id;
    wsrep_buf_t err_buf = { NULL, 0 };
    int const app_err = node_store_apply(store, &trx_id, ws);
    if (app_err)
    {
        /* REPLICATION: if applying failed, prepare an error buffer with
         *              sufficient error specification */
        err_buf.ptr = &app_err; // suppose error code is enough
        err_buf.len = sizeof(app_err);
    }

    wsrep_status_t ret;
    ret = wsrep->commit_order_enter(wsrep, ws_handle, ws_meta);
    if (ret) {
        node_store_rollback(store, trx_id);
        return ret;
    }

    if (!app_err) node_store_commit(store, trx_id, &ws_meta->gtid);
    else          node_store_update_gtid(store, &ws_meta->gtid);

    ret = wsrep->commit_order_leave(wsrep, ws_handle, ws_meta, &err_buf);

    return ret;
}
