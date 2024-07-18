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

/** @file wsrep_connection_monitor_service.h
 *
 * This file defines interface for connection monitor service
 *
 * The provider which is capable of using the service interface v1 must
 * export the following functions.
 *
 * int wsrep_init_connection_monitor_service_v1(wsrep_connection_monitor_service_v1_t*)
 * void wsrep_deinit_connection_monitor_service_v1()
 *
 * which can be probed by the application.
 *
 * The application must initialize the service via above init function
 * before the provider is initialized via wsrep->init(). The deinit
 * function must be called after the provider side resources have been
 * released via wsrep->free().
 */

#ifndef WSREP_CONNECTION_MONITOR_SERVICE_H
#define WSREP_CONNECTION_MONITOR_SERVICE_H

#include "wsrep_api.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/**
 * Type tag for application defined connection monitoring processing context.
 *
 * Application may pass pointer to the context when initializing
 * the connection monitor service. This pointer is passed a first parameter for
 * each service call.
 */
typedef struct wsrep_connection_monitor_context wsrep_connection_monitor_context_t;

typedef enum
{
    WSREP_CONNECTION_MONITOR_CONNECT = 1,
    WSREP_CONNECTION_MONITOR_DISCONNECT
} wsrep_connection_monitor_key_t;

/*
 * Connection monitor connection update callback.
 *
 */
typedef void (*wsrep_connection_monitor_cb_t)(
    wsrep_connection_monitor_context_t*,
    wsrep_connection_monitor_key_t key,
    const wsrep_buf_t* remote_uuid,
    const wsrep_buf_t* connection_scheme,
    const wsrep_buf_t* connection_address);

/**
 * Connection monitor service struct.
 *
 * A pointer to this struct must be passed to the call to
 * wsrep_init_connection_monitor_service_v1.
 *
 * The application must provide implementation to all functions defined
 * in this struct.
 */
typedef struct wsrep_connection_monitor_service_v1_st
{
    /* Connection monitor update callback */
    wsrep_connection_monitor_cb_t connection_monitor_cb;
    /* Pointer to application defined connection monitor context. */
    wsrep_connection_monitor_context_t* context;
} wsrep_connection_monitor_service_v1_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */


#define WSREP_CONNECTION_MONITOR_SERVICE_INIT_FUNC_V1 "wsrep_init_connection_monitor_service_v1"
#define WSREP_CONNECTION_MONITOR_SERVICE_DEINIT_FUNC_V1 "wsrep_deinit_connection_monitor_service_v1"

#endif /* WSREP_CONNECTION_MONITOR_SERVICE_H */
