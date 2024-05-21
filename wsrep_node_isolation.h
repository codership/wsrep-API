/*
 * Copyright (C) 2024 Codership Oy <info@codership.com>
 *
 * This file is part of wsrep-API.
 *
 * Wsrep-API is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Wsrep-API is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wsrep-API.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef WSREP_NODE_ISOLATION_H
#define WSREP_NODE_ISOLATION_H

/** @file wsrep_node_isolation.h
 *
 * This file defines and interface to isolate the node from
 * the rest of the cluster. The purpose of isolation is to shut
 * down all communication with the rest of the cluster in case
 * of node failure where the node cannot continue reliably anymore,
 * e.g. in case of handling a signal which will terminate the process.
 */

/**
 * Mode of node isolation.
 */
enum wsrep_node_isolation_mode
{
    /** Node is not isolated. */
    WSREP_NODE_ISOLATION_NOT_ISOLATED,
    /** Node is isolated from the rest of the cluster on network
     * level. All ongoing network connections will be terminated and
     * no new connections are accepted. */
    WSREP_NODE_ISOLATION_ISOLATED,
    /** As WSREP_NODE_ISOLATION_ON, but also force the provider
     * to deliver view with status WSREP_VIEW_DISCONNECTED. */
    WSREP_NODE_ISOLATION_FORCE_DISCONNECT,
};

enum wsrep_node_isolation_result
{
    /** Setting the isolation mode was successful. */
    WSREP_NODE_ISOLATION_SUCCESS,
    /** Invalid isolation mode was passed. */
    WSREP_NODE_ISOLATION_INVALID_VALUE
};

/** Set mode isolation mode according to give wsrep_node_isolation_mode
 * enum.
 *
 * The implementation must be async signal safe to allow calling
 * it from program signal handler.
 *
 * @param mode Mode to set.
 * @return wsrep_node_isolation_result enum.
 */
typedef enum wsrep_node_isolation_result (*wsrep_node_isolation_mode_set_fn_v1)(
    enum wsrep_node_isolation_mode mode);

#define WSREP_NODE_ISOLATION_MODE_SET_V1 "wsrep_node_isolation_mode_set_v1"

#endif /* WSREP_NODE_ISOLATION_H */
