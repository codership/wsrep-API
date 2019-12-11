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

/**
 * @file This unit defines simple "transactional storage engine" interface
 */

#ifndef NODE_STORE_H
#define NODE_STORE_H

#include "options.h"

#include "../../wsrep_api.h"

typedef struct node_store node_store_t;

/**
 * open a store and optionally assocoate a file with it */
extern node_store_t*
node_store_open(const struct node_options* opts);

/**
 * close store and deallocate associated resources */
extern void
node_store_close(node_store_t* store);

/**
 * initialize store (assign current GTID) */
extern int
node_store_init(node_store_t* store, const wsrep_gtid_t* gtid);

/**
 * get the current GTID (last committed) */
extern void
node_store_gtid(node_store_t* store, wsrep_gtid_t* gtid);

/**
 * execute and prepare local transaction in store and return its key and write
 * set.
 *
 * This operation allocates resources that must be freed with either
 * node_store_commit() or node_store_rollback()
 *
 * @param[out] trx_id locally unique transaction ID
 * @param[out] key    key that uniquely identifies modified resource (record)
 * @param[out] ws     write set that countains description of the transaction
 *                    changes sufficient for applying on the slave
 */
extern int
node_store_execute(node_store_t*   store,
                   wsrep_trx_id_t* trx_id,
                   wsrep_key_t*    key,
                   wsrep_buf_t*    ws);

/**
 * apply and prepare foreign write set received from replication
 *
 * This operation allocates resources that must be freed with either
 * node_store_commit() or node_store_rollback()
 *
 * @param[out] trx_id locally unique transaction ID
 * @param[in]  ws     foreign transaction write set
 */
extern int
node_store_apply(node_store_t*      store,
                 wsrep_trx_id_t*    trx_id,
                 const wsrep_buf_t* ws);

/**
 * commit prepared transaction identified by trx_id */
extern void
node_store_commit(node_store_t*       store,
                  wsrep_trx_id_t      trx_id,
                  const wsrep_gtid_t* ws_gtid);

/**
 * rollback prepared transaction identified by trx_id */
extern void
node_store_rollback(node_store_t*  store,
                    wsrep_trx_id_t trx_id);

/**
 * update storage GTID for transactions that had to be skipped/rolled back */
extern void
node_store_update_gtid(node_store_t*       store,
                       const wsrep_gtid_t* ws_gtid);

#endif /* NODE_STORE_H */
