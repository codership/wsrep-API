/*
 * Copyright (C) 2024-2025 Codership Oy <info@codership.com>
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

typedef const void* wsrep_connection_key_t;

/*
 * Connection monitor connection update callbacks.
 *
 */

/*
 * Connection connect callback.
 *
 * @param id connection identifier
 * @param scheme connection scheme (tcp, ssl)
 * @param local_address local address string
 * @param remote_address remote address string
 */
typedef void (*wsrep_connection_monitor_connect_cb_t)(
    wsrep_connection_monitor_context_t*,
    wsrep_connection_key_t id,
    const wsrep_buf_t* scheme,
    const wsrep_buf_t* local_address,
    const wsrep_buf_t* remote_address);

/*
 * Connection disconnect callback.
 *
 * @param id connection identifier
 */
typedef void (*wsrep_connection_monitor_disconnect_cb_t)(
    wsrep_connection_monitor_context_t*,
    wsrep_connection_key_t id);

/*
 * Connection SSL/TLS info callback.
 *
 * @param id connection identifier
 * @param cipher cipher suite name
 * @param certificate_subject certificate subject name
 * @param certificate_issuer certificate issuer name
 * @param version SSL/TLS version string
 */
typedef void (*wsrep_connection_monitor_ssl_info_cb_t)(
    wsrep_connection_monitor_context_t*,
    wsrep_connection_key_t id,
    const wsrep_buf_t* cipher,
    const wsrep_buf_t* certificate_subject,
    const wsrep_buf_t* certificate_issuer,
    const wsrep_buf_t* version);

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
    /* Connection monitor connect callback */
    wsrep_connection_monitor_connect_cb_t connection_monitor_connect_cb;
    /* Connection monitor disconnect callback */
    wsrep_connection_monitor_disconnect_cb_t connection_monitor_disconnect_cb;
    /* Connection monitor ssl info callback */
    wsrep_connection_monitor_ssl_info_cb_t connection_monitor_ssl_info_cb;
    /* Pointer to application defined connection monitor context. */
    wsrep_connection_monitor_context_t* context;
} wsrep_connection_monitor_service_v1_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */


#define WSREP_CONNECTION_MONITOR_SERVICE_INIT_FUNC_V1 "wsrep_init_connection_monitor_service_v1"
#define WSREP_CONNECTION_MONITOR_SERVICE_DEINIT_FUNC_V1 "wsrep_deinit_connection_monitor_service_v1"

#endif /* WSREP_CONNECTION_MONITOR_SERVICE_H */
