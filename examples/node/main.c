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

#include "ctx.h"
#include "log.h"
#include "options.h"
#include "stats.h"
#include "worker.h"
#include "wsrep.h"

#include <string.h> // strerror()

int main(int argc, char* argv[])
{
    struct node_options opts;
    int err = node_options_read(argc, argv, &opts);
    if (err)
    {
        NODE_FATAL("Failed to read command line opritons: %d (%s)",
                   err, strerror(err));
        return err;
    }

    struct node_ctx node;
    node.opts = &opts;

    /* REPLICATION: before connecting to cluster we need to initialize our
     *              storage to know our current position (GTID) */
    node.store = node_store_open(&opts);
    if (!node.store)
    {
        NODE_FATAL("Failed to open node store");
        return 1;
    }

    wsrep_gtid_t current_gtid;
    node_store_gtid(node.store, &current_gtid);

    /* REPLICATION: now we can connect to the cluster and start receiving
     *              replication events */
    node.wsrep = node_wsrep_open(&opts, &current_gtid, &node);
    if (!node.wsrep)
    {
        NODE_FATAL("Failed to open wsrep provider");
        return 1;
    }

    /* REPLICATION: and start processing replicaiton events */
    struct node_worker_pool* slave_pool =
        node_worker_start(&node, NODE_WORKER_SLAVE, (size_t)opts.slaves);
    if (!slave_pool)
    {
        NODE_FATAL("Failed to create slave worker pool");
        return 1;
    }

    /* REPLICATION: now that replicaton events are being processed we can
     *              wait to sync with the cluster */
    if (!node_wsrep_wait_synced(node.wsrep))
    {
        NODE_ERROR("Failed to wait fir SYNCED event");
        return 1;
    }

    NODE_INFO("Synced with cluster");

    /* REPLICATION: now we can start replicate own events */
    struct node_worker_pool* master_pool =
        node_worker_start(&node, NODE_WORKER_MASTER, (size_t)opts.masters);
    if (opts.masters > 0 && !master_pool)
    {
        NODE_FATAL("Failed to create master worker pool");
        return 1;
    }

    node_stats_loop(node_wsrep_provider(node.wsrep), (int)opts.period);

    /* REPLICATON: to shut down we go in the opposite order:
     *             first  - shutdown master threads,
     *             second - close provider,
     *             and then wait for slave threads to join */
    node_worker_stop(master_pool);

    node_wsrep_close(node.wsrep);

    node_worker_stop(slave_pool);

    /* and finally, when the storage can no longer be disturbed, close it */
    node_store_close(node.store);

    return 0;
}
