/* Copyright (C) 2009-2013 Codership Oy <info@codership.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/** @file wsrep_api.h WSREP API declaration. */

/*
  HOW TO READ THIS FILE.

  Due to C language rules this header layout doesn't lend itself to intuitive
  reading. So here's the scoop: in the end this header declares two main types:

  * struct wsrep_init_args

  and

  * struct wsrep

  wsrep_init_args contains initialization parameters for wsrep provider like
  names, addresses, etc. and pointers to callbacks. The callbacks will be called
  by provider when it needs to do something application-specific, like log a
  message or apply a writeset. It should be passed to init() call from
  wsrep API. It is an application part of wsrep API contract.

  struct wsrep is the interface to wsrep provider. It contains all wsrep API
  calls. It is a provider part of wsrep API contract.

  Finally, wsrep_load() method loads (dlopens) wsrep provider library. It is
  defined in wsrep_loader.c unit and is part of libwsrep.a (which is not a
  wsrep provider, but a convenience library).

  wsrep_unload() does the reverse.

*/
#ifndef WSREP_H
#define WSREP_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************
 *                                                                        *
 *                       wsrep replication API                            *
 *                                                                        *
 **************************************************************************/

/** Interface version number. */
#define WSREP_INTERFACE_VERSION "26"

/** Empty backend spec. */
#define WSREP_NONE "none"


/** Log severity levels, passed as first argument to log handler.
 */
typedef enum wsrep_log_level
{
    /** Unrecoverable error, application must quit. */
    WSREP_LOG_FATAL,

    /** Operation failed, must be repeated. */
    WSREP_LOG_ERROR,

    /** Unexpected condition, but no operational failure. */
    WSREP_LOG_WARN,

    /** Informational message. */
    WSREP_LOG_INFO,

    /** Debug message. Shows only if compiled with debug. */
    WSREP_LOG_DEBUG
} wsrep_log_level_t;

/** Error log handler.
 * All messages from wsrep provider are directed to this handler, if present.
 * @param[in] level log level
 * @param[in] message log message */
typedef void (*wsrep_log_cb_t)(wsrep_log_level_t, const char *);


/** Provider capabilities application may want to know about. */

/** Multi master. */
#define WSREP_CAP_MULTI_MASTER          ( 1ULL << 0 )

/** Certification. */
#define WSREP_CAP_CERTIFICATION         ( 1ULL << 1 )

/** Parallel applying. */
#define WSREP_CAP_PARALLEL_APPLYING     ( 1ULL << 2 )

/** Transaction replay. */
#define WSREP_CAP_TRX_REPLAY            ( 1ULL << 3 )

/** Isolation. */
#define WSREP_CAP_ISOLATION             ( 1ULL << 4 )

/** Pause. */
#define WSREP_CAP_PAUSE                 ( 1ULL << 5 )

/** Causal reads. */
#define WSREP_CAP_CAUSAL_READS          ( 1ULL << 6 )

/** Causal trx. */
#define WSREP_CAP_CAUSAL_TRX            ( 1ULL << 7 )

/** Incremental writeset. */
#define WSREP_CAP_INCREMENTAL_WRITESET  ( 1ULL << 8 )

/** Session locks. */
#define WSREP_CAP_SESSION_LOCKS         ( 1ULL << 9 )

/** Distributed locks. */
#define WSREP_CAP_DISTRIBUTED_LOCKS     ( 1ULL << 10 )

/** Consistency check. */
#define WSREP_CAP_CONSISTENCY_CHECK     ( 1ULL << 11 )

/** Unordered. */
#define WSREP_CAP_UNORDERED             ( 1ULL << 12 )

/** Annotation. */
#define WSREP_CAP_ANNOTATION            ( 1ULL << 13 )

/** Preordered. */
#define WSREP_CAP_PREORDERED            ( 1ULL << 14 )

/** Streaming. */
#define WSREP_CAP_STREAMING             ( 1ULL << 15 )

/** Snapshot. */
#define WSREP_CAP_SNAPSHOT              ( 1ULL << 16 )

/** NBO. */
#define WSREP_CAP_NBO                   ( 1ULL << 17 )

/** Capabilities bitmask. */
typedef uint32_t wsrep_cap_t;

/** Writeset flags.
 * Note that some of the flags are mutually exclusive (e.g. TRX_END and
 * ROLLBACK). */

/** The writeset and all preceding writesets must be committed. */
#define WSREP_FLAG_TRX_END              ( 1ULL << 0 )

/** All preceding writesets in a transaction must be rolled back. */
#define WSREP_FLAG_ROLLBACK             ( 1ULL << 1 )

/** The writeset must be applied AND committed in isolation. */
#define WSREP_FLAG_ISOLATION            ( 1ULL << 2 )

/** The writeset cannot be applied in parallel. */
#define WSREP_FLAG_PA_UNSAFE            ( 1ULL << 3 )

/** The order in which the writeset is applied does not matter. */
#define WSREP_FLAG_COMMUTATIVE          ( 1ULL << 4 )

/** The writeset contains another writeset in this provider format. */
#define WSREP_FLAG_NATIVE               ( 1ULL << 5 )

/** Shall be set on the first trx fragment by provider. */
#define WSREP_FLAG_TRX_START            ( 1ULL << 6 )

/** Shall be set on the fragment which prepares the transaction. */
#define WSREP_FLAG_TRX_PREPARE          ( 1ULL << 7 )

/** Snapshot. */
#define WSREP_FLAG_SNAPSHOT             ( 1ULL << 8 )

/** Equal to the last/biggest WSREP_FLAG_*. */
#define WSREP_FLAGS_LAST                WSREP_FLAG_SNAPSHOT

/** Bitmask to extract any WSREP_FLAG_*. */
#define WSREP_FLAGS_MASK                ((WSREP_FLAGS_LAST << 1) - 1)


/** Application transaction ID. */
typedef uint64_t wsrep_trx_id_t;

/** Application connection ID. */
typedef uint64_t wsrep_conn_id_t;

/** sequence number of a writeset, etc. */
typedef int64_t  wsrep_seqno_t;

/** Boolean type. Should be the same as standard (C99) bool. */
#ifdef __cplusplus
typedef bool     wsrep_bool_t;
#else
typedef _Bool    wsrep_bool_t;
#endif /* __cplusplus */

/** Undefined seqno. */
#define WSREP_SEQNO_UNDEFINED (-1)


/** WSREP provider status codes. */
typedef enum wsrep_status
{
    /** Success */
    WSREP_OK = 0,

    /** Minor warning, error logged. */
    WSREP_WARNING,

    /** Transaction is not known by wsrep. */
    WSREP_TRX_MISSING,

    /** Transaction aborted, server can continue. */
    WSREP_TRX_FAIL,

    /** Trx was victim of brute force abort. */
    WSREP_BF_ABORT,

    /** Data exceeded maximum supported size. */
    WSREP_SIZE_EXCEEDED,

    /** Error in client connection, must abort. */
    WSREP_CONN_FAIL,

    /** Error in node state, wsrep must reinit. */
    WSREP_NODE_FAIL,

    /** Fatal error, server must abort. */
    WSREP_FATAL,

    /** Feature not implemented. */
    WSREP_NOT_IMPLEMENTED,

    /** Operation not allowed. */
    WSREP_NOT_ALLOWED
} wsrep_status_t;


/** WSREP callbacks status codes.
 * Technically, wsrep provider has no use for specific failure codes since
 * there is nothing it can do about it but abort execution. Therefore any
 * positive number shall indicate a critical failure. Optionally that value
 * may be used by provider to come to a consensus about state consistency
 * in a group of nodes. */
typedef enum wsrep_cb_status
{
    /** Success (as in "not critical failure"). */
    WSREP_CB_SUCCESS = 0,

    /** Critical failure (consistency violation). */
    WSREP_CB_FAILURE
} wsrep_cb_status_t;


/** UUID type - for all unique IDs. */
typedef union wsrep_uuid {
    /** Actual data. */
    uint8_t data[16];

    /** Present to enforce alignment. */
    size_t  alignment;
} wsrep_uuid_t;

/** Undefined UUID. */
static const wsrep_uuid_t WSREP_UUID_UNDEFINED = {{0,}};

/** UUID string representation length, terminating '\0' not included. */
#define WSREP_UUID_STR_LEN 36

/** Scan UUID from string.
 * @param[in] str UUID as a string
 * @param[in] str_len `str` length
 * @param[out] uuid parsed UUID as a struct
 * @return length of UUID string representation or negative error code */
extern int
wsrep_uuid_scan (const char* str, size_t str_len, wsrep_uuid_t* uuid);

/** Print UUID to string.
 * @param[in] uuid UUID as a struct
 * @param[out] str output string
 * @param[in] str_len `str` max size
 * @return length of UUID string representation or negative error code */
extern int
wsrep_uuid_print (const wsrep_uuid_t* uuid, char* str, size_t str_len);

/** Compare two UUIDs.
 * Performs a byte by byte comparison of lhs and rhs.
 * Returns 0 if lhs and rhs match, otherwise -1 or 1 according to the
 * difference of the first byte that differs in lsh and rhs.
 * @param[in] lhs left hand side
 * @param[in] rhs right hand side
 * @return -1, 0, 1 if lhs is respectively smaller, equal, or greater than rhs
 */
extern int
wsrep_uuid_compare (const wsrep_uuid_t* lhs, const wsrep_uuid_t* rhs);

/** Maximum logical member name length. */
#define WSREP_MEMBER_NAME_LEN 32

/** Max Domain Name length + 0x00. */
#define WSREP_INCOMING_LEN 256


/** Global transaction identifier. */
typedef struct wsrep_gtid
{
    /** History UUID. */
    wsrep_uuid_t  uuid;

    /** Sequence number. */
    wsrep_seqno_t seqno;
} wsrep_gtid_t;

/** Undefined GTID. */
static const wsrep_gtid_t WSREP_GTID_UNDEFINED = {{{0, }}, -1};

/** Minimum number of bytes guaranteed to store GTID string representation.
 * Terminating '\0' not included (36 + 1 + 20). */
#define WSREP_GTID_STR_LEN 57


/** Read GTID from string.
 * @param[in] str gtid string input
 * @param[in] str_len length of `str`
 * @param[out] gtid parsed gtid
 * @return length of GTID string representation or -EINVAL in case of error */
extern int
wsrep_gtid_scan(const char* str, size_t str_len, wsrep_gtid_t* gtid);

/** Write GTID to string.
 * The result is always '\0'-terminated.
 * @param[in] gtid gtid to be converted to a string
 * @param[out] str the result will be written to the location pointed by this
 * @param[in] str_len size of `str`
 * @return length of GTID string representation or -EMSGSIZE if the output
 * buffer is too short */
extern int
wsrep_gtid_print(const wsrep_gtid_t* gtid, char* str, size_t str_len);

/** Source/server transaction ID (trx ID assigned at originating node). */
typedef struct wsrep_stid {
    /** Source node ID. */
    wsrep_uuid_t node;

    /** Local trx ID at source. */
    wsrep_trx_id_t trx;

    /** Local connection ID at source. */
    wsrep_conn_id_t conn;
} wsrep_stid_t;

/** Transaction meta data. */
typedef struct wsrep_trx_meta
{
    /** Global transaction identifier. */
    wsrep_gtid_t gtid;

    /** Source transaction identifier. */
    wsrep_stid_t stid;

    /** Sequence number of the last transaction this transaction may
     * depend on. */
    wsrep_seqno_t depends_on;
} wsrep_trx_meta_t;

/** Abstract data buffer structure. */
typedef struct wsrep_buf
{
    /** Pointer to data buffer. */
    const void* ptr;

    /** Length of buffer. */
    size_t len;
} wsrep_buf_t;

/** Transaction handle struct passed for wsrep transaction handling calls */
typedef struct wsrep_ws_handle
{
    /** Transaction ID. */
    wsrep_trx_id_t trx_id;

    /** Opaque provider transaction context data. */
    void* opaque;
} wsrep_ws_handle_t;

/** Member state. */
typedef enum wsrep_member_status {
    /** Undefined state. */
    WSREP_MEMBER_UNDEFINED,

    /** Incomplete state. Requested state transfer. */
    WSREP_MEMBER_JOINER,

    /** Complete state. Donates state transfer. */
    WSREP_MEMBER_DONOR,

    /** Complete state. */
    WSREP_MEMBER_JOINED,

    /** Complete state. Synchronized with group. */
    WSREP_MEMBER_SYNCED,

    /** Provider-specific error code. */
    WSREP_MEMBER_ERROR,

    /** Max/biggest WSREP_MEMBER_*. */
    WSREP_MEMBER_MAX
} wsrep_member_status_t;

/** Static information about a group member (some fields are tentative yet). */
typedef struct wsrep_member_info {
    /** Group-wide unique member ID. */
    wsrep_uuid_t id;

    /** Human-readable name. */
    char name[WSREP_MEMBER_NAME_LEN];

    /** Address for client requests. */
    char incoming[WSREP_INCOMING_LEN];
} wsrep_member_info_t;

/** Group status. */
typedef enum wsrep_view_status {
    /** Primary group configuration (quorum present). */
    WSREP_VIEW_PRIMARY,

    /** Non-primary group configuration (quorum lost). */
    WSREP_VIEW_NON_PRIMARY,

    /** Not connected to group, retrying. */
    WSREP_VIEW_DISCONNECTED,

    /** Max/biggest WSREP_VIEW_*. */
    WSREP_VIEW_MAX
} wsrep_view_status_t;

/** View of the group. */
typedef struct wsrep_view_info {
    /** Global state ID. */
    wsrep_gtid_t state_id;

    /** Global view number. */
    wsrep_seqno_t view;

    /** View status. */
    wsrep_view_status_t status;

    /** Capabilities available in the view. */
    wsrep_cap_t capabilities;

    /** Index of this member in the view. */
    int my_idx;

    /** Number of members in the view. */
    int memb_num;

    /** Application protocol agreed on the view. */
    int proto_ver;

    /** Array of member information. */
    wsrep_member_info_t members[1];
} wsrep_view_info_t;


/** Connected to group.
 * This handler is called once the first primary view is seen.
 * The purpose of this call is to provide basic information only,
 * like node UUID and group UUID. */
typedef enum wsrep_cb_status (*wsrep_connected_cb_t) (
    void*                    app_ctx,
    const wsrep_view_info_t* view
);


/** Group view handler.
 * This handler is called in *total order* corresponding to the group
 * configuration change. It is to provide a vital information about
 * new group view.
 * @param[in,out] app_ctx application context
 * @param[in,out] recv_ctx receiver context
 * @param[in] view new view on the group
 * @param[in] state current state
 * @param[in] state_len length of current state
 */
typedef enum wsrep_cb_status (*wsrep_view_cb_t) (
    void*                     app_ctx,
    void*                     recv_ctx,
    const wsrep_view_info_t*  view,
    const char*               state,
    size_t                    state_len
);


/** Magic string to tell provider to engage into trivial (empty) state transfer.
 * No data will be passed, but the node shall be considered JOINED.
 * Should be passed in sst_req parameter of wsrep_sst_cb_t. */
#define WSREP_STATE_TRANSFER_TRIVIAL "trivial"

/** Magic string to tell provider not to engage in state transfer at all.
 * The member will stay in WSREP_MEMBER_UNDEFINED state but will keep on
 * receiving all writesets.
 * Should be passed in sst_req parameter of wsrep_sst_cb_t. */
#define WSREP_STATE_TRANSFER_NONE "none"


/** Creates and returns State Snapshot Transfer request for provider.
 * This handler is called whenever the node is found to miss some of events
 * from the cluster history (e.g. fresh node joining the cluster).
 * SST will be used if it is impossible (or impractically long) to replay
 * missing events, which may be not known in advance, so the node must always
 * be ready to accept full SST or abort in case event replay is impossible.
 *
 * Normally SST request is an opaque buffer that is passed to the
 * chosen SST donor node and must contain information sufficient for
 * donor to deliver SST (typically SST method and delivery address).
 * See above macros WSREP_STATE_TRANSFER_TRIVIAL and WSREP_STATE_TRANSFER_NONE
 * to modify the standard provider behavior.
 *
 * @note Currently it is assumed that sst_req is allocated using
 *       malloc()/calloc()/realloc() and it will be freed by wsrep provider.
 *
 * @param[in] app_ctx application context
 * @param[out] sst_req location to store SST request
 * @param[out] sst_req_len location to store SST request length or error code,
 * value of 0 means no SST.
 */
typedef enum wsrep_cb_status (*wsrep_sst_request_cb_t) (
    void*                     app_ctx,
    void**                    sst_req,
    size_t*                   sst_req_len
);


/** Apply callback.
 * This handler is called from wsrep library to apply replicated writeset
 * Must support brute force applying for multi-master operation
 * @param[in] recv_ctx receiver context pointer provided by the application
 * @param[in] ws_handle internal provider writeset handle
 * @param[in] flags WSREP_FLAG_... flags
 * @param[in] data data buffer containing the writeset
 * @param[in] meta transaction meta data of the writeset to be applied
 * @param[out] exit_loop set to true to exit receive loop
 * @return error code:
 * @retval 0 - success
 * @retval non-0 - application-specific error code
 */
typedef enum wsrep_cb_status (*wsrep_apply_cb_t) (
    void*                    recv_ctx,
    const wsrep_ws_handle_t* ws_handle,
    uint32_t                 flags,
    const wsrep_buf_t*       data,
    const wsrep_trx_meta_t*  meta,
    wsrep_bool_t*            exit_loop
);


/** Unordered callback.
 * This handler is called to execute unordered actions (actions that need not
 * to be executed in any particular order) attached to writeset.
 * @param[out] recv_ctx receiver context pointer provided by the application
 * @param[in] data data buffer containing the writeset
 */
typedef enum wsrep_cb_status (*wsrep_unordered_cb_t) (
    void*              recv_ctx,
    const wsrep_buf_t* data
);


/** A callback to donate state snapshot.
 * This handler is called from wsrep library when it needs this node
 * to deliver state to a new cluster member.
 * No state changes will be committed for the duration of this call.
 * Wsrep implementation may provide internal state to be transmitted
 * to new cluster member for initial state.
 * @param[out] app_ctx application context
 * @param[out] recv_ctx receiver context
 * @param[in] str_msg state transfer request message
 * @param[in] gtid current state ID on this node
 * @param[in] state current wsrep internal state buffer
 * @param[in] bypass bypass snapshot transfer, only transfer uuid:seqno pair */
typedef enum wsrep_cb_status (*wsrep_sst_donate_cb_t) (
    void*               app_ctx,
    void*               recv_ctx,
    const wsrep_buf_t*  str_msg,
    const wsrep_gtid_t* state_id,
    const wsrep_buf_t*  state,
    wsrep_bool_t        bypass
);


/** A callback to signal application that wsrep state is synced with cluster.
 * This callback is called after wsrep library has got in sync with
 * rest of the cluster.
 * @param[in,out] app_ctx application context
 * @return wsrep_cb_status enum */
typedef enum wsrep_cb_status (*wsrep_synced_cb_t) (void* app_ctx);


/** Initialization parameters for wsrep provider. */
struct wsrep_init_args
{
    /** Application context for callbacks. */
    void* app_ctx;

    /** @name Configuration parameters. */
    /** @{ */
    /** Symbolic name of this node (eg hostname). */
    const char* node_name;

    /** Address to be used by wsrep provider. */
    const char* node_address;

    /** Address for incoming client connections. */
    const char* node_incoming;

    /** Directory where wsrep files are kept if any. */
    const char* data_dir;

    /** Provider-specific configuration string. */
    const char* options;

    /** Max supported application protocol version. */
    int proto_ver;
    /** @} */

    /** @name Application initial state information. */
    /** @{ */
    /** Application state GTID. */
    const wsrep_gtid_t* state_id;

    /** Initial state for wsrep provider. */
    const wsrep_buf_t* state;
    /** @} */

    /** @name Application callbacks. */
    /** @{ */
    /** Logging handler. */
    wsrep_log_cb_t logger_cb;

    /** Connected to group. */
    wsrep_connected_cb_t connected_cb;

    /** Group view change handler. */
    wsrep_view_cb_t view_cb;

    /** SST request creator. */
    wsrep_sst_request_cb_t sst_request_cb;
    /** @} */

    /** @name Applier callbacks. */
    /** @{ */
    /** Apply callback. */
    wsrep_apply_cb_t apply_cb;

    /** Callback for unordered actions. */
    wsrep_unordered_cb_t unordered_cb;
    /** @} */

    /** @name State Snapshot Transfer callbacks. */
    /** @{ */
    /** Donate SST. */
    wsrep_sst_donate_cb_t sst_donate_cb;

    /** Synced with group. */
    wsrep_synced_cb_t synced_cb;
    /** @} */
};


/** Type of the stats variable value in struct wsrep_status_var. */
typedef enum wsrep_var_type
{
    /** Pointer to null-terminated string. */
    WSREP_VAR_STRING,

    /** 64 bit signed integer. */
    WSREP_VAR_INT64,

    /** Double. */
    WSREP_VAR_DOUBLE
}
wsrep_var_type_t;

/** Generalized stats variable representation */
struct wsrep_stats_var
{
    /** Variable name. */
    const char* name;

    /** Variable value type. */
    wsrep_var_type_t type;

    /** Variable value. */
    union {
        /** Used if type is `WSREP_VAR_INT64`. */
        int64_t _int64;

        /** Used if type is `WSREP_VAR_DOUBLE`. */
        double _double;

        /** Used if type is `WSREP_VAR_STRING`. */
        const char* _string;
    } value;
};


/** Key struct used to pass certification keys for transaction handling calls.
 * A key consists of zero or more key parts. */
typedef struct wsrep_key
{
    /** Array of key parts. */
    const wsrep_buf_t* key_parts;

    /** Number of key parts. */
    size_t key_parts_num;
} wsrep_key_t;

/** Certification key type.
 * Compatibility table (C - conflict, N - no conflict):
 *
 * first \ second | EXCLUSIVE   | SHARED
 * :------------- | :---------: | :---------:
 * EXCLUSIVE      | C           | C
 * SHARED         | N           | N
 */
typedef enum wsrep_key_type
{
    /** Shared */
    WSREP_KEY_SHARED = 0,

    /** Reserved. If not supported, should be interpreted as EXCLUSIVE. */
    WSREP_KEY_SEMI,

    /** Exclusive */
    WSREP_KEY_EXCLUSIVE
} wsrep_key_type_t;

/** Data type. */
typedef enum wsrep_data_type
{
    /** State modification event that should be applied and committed in
     * order. */
    WSREP_DATA_ORDERED = 0,

    /** Some action that does not modify state and execution of which is
     * optional and does not need to happen in order. */
    WSREP_DATA_UNORDERED,

    /** Human readable writeset annotation. */
    WSREP_DATA_ANNOTATION
} wsrep_data_type_t;


/** Helper method to reset trx writeset handle state when trx id changes.
 * Instead of passing wsrep_ws_handle_t directly to wsrep calls, wrapping
 * handle with this call offloads bookkeeping from application. */
static inline wsrep_ws_handle_t* wsrep_ws_handle_for_trx(
    wsrep_ws_handle_t* ws_handle,
    wsrep_trx_id_t     trx_id)
{
    if (ws_handle->trx_id != trx_id)
    {
        ws_handle->trx_id = trx_id;
        ws_handle->opaque = NULL;
    }
    return ws_handle;
}


/** Handle for processing preordered actions.
 * Must be initialized to `WSREP_PO_INITIALIZER` before use. */
typedef struct wsrep_po_handle {
    /** Opaque provider context data. */
    void* opaque;
} wsrep_po_handle_t;

/** wsrep_po_handle default value. */
static const wsrep_po_handle_t WSREP_PO_INITIALIZER = { NULL };


/** WSREP interface for dynamically loadable libraries (typedef). */
typedef struct wsrep_st wsrep_t;

/** WSREP interface for dynamically loadable libraries (struct). */
struct wsrep_st {

    const char *version; //!< interface version string

    /** Initializes wsrep provider.
     * @param[out] wsrep provider handle
     * @param[in] args wsrep initialization parameters */
    wsrep_status_t (*init)   (wsrep_t*                      wsrep,
                              const struct wsrep_init_args* args);

    /** Returns provider capabilities bitmap.
     * Note that these are potential provider capabilities. Provider will
     * offer only capabilities supported by all members in the view
     * (see wsrep_view_info).
     * @param[in,out] wsrep provider handle */
    wsrep_cap_t    (*capabilities) (wsrep_t* wsrep);

    /** Passes provider-specific configuration string to provider.
     * @param[in,out] wsrep provider handle
     * @param[in] conf configuration string
     * @retval WSREP_OK configuration string was parsed successfully
     * @retval WSREP_WARNING could not parse configuration string, no action taken
     */
    wsrep_status_t (*options_set) (wsrep_t* wsrep, const char* conf);

    /** Returns provider-specific string with current configuration values.
     * @param[in,out] wsrep provider handle
     * @return a dynamically allocated string with current configuration
     * parameter values */
    char*          (*options_get) (wsrep_t* wsrep);

    /** Opens connection to cluster.
     * Returns when either node is ready to operate as a part of the cluster
     * or fails to reach operating status.
     * @param[in,out] wsrep provider handle
     * @param[in] cluster_name unique symbolic cluster name
     * @param[in] cluster_url URL-like cluster address (backend://address)
     * @param[in] state_donor name of the node to be asked for state transfer.
     * @param[in] bootstrap a flag to request initialization of a new wsrep
     * service rather then a connection to the existing one. `cluster_url` may
     * still carry important initialization parameters, like backend spec
     * and/or listen address. */
    wsrep_status_t (*connect) (wsrep_t*     wsrep,
                               const char*  cluster_name,
                               const char*  cluster_url,
                               const char*  state_donor,
                               wsrep_bool_t bootstrap);

    /** Closes connection to cluster.
     * If state_uuid and/or state_seqno is not NULL, will store final state
     * in there.
     * @param[in,out] wsrep this wsrep handler */
    wsrep_status_t (*disconnect)(wsrep_t* wsrep);

    /** Start receiving replication events.
     * This function never returns
     * @param[in] wsrep provider handle
     * @param[in] recv_ctx receiver context */
    wsrep_status_t (*recv)(wsrep_t* wsrep, void* recv_ctx);

    /** Tells provider that a given writeset has a read view associated with it.
     * @param[in,out] wsrep provider handle
     * @param[in,out] handle writeset handle
     * @param[in] rv read view GTID established by the caller or if NULL,
     * provider will infer it internally. */
    wsrep_status_t (*assign_read_view)(wsrep_t*            wsrep,
                                       wsrep_ws_handle_t*  handle,
                                       const wsrep_gtid_t* rv);

    /** Certifies transaction with provider.
     * Must be called before transaction commit. Returns success code, which
     * caller must check.
     *
     * In case of WSREP_OK, transaction can proceed to commit.
     * Otherwise transaction must rollback.
     *
     * In case of a failure there are two conceptually different situations:
     * - the writeset was not ordered. In that case meta struct shall contain
     *   undefined GTID: WSREP_UUID_UNDEFINED:WSREP_SEQNO_UNDEFINED.
     * - the writeset was successfully ordered, but failed certification.
     *   In this case meta struct shall contain a valid GTID.
     *
     * Regardless of the return code, if meta struct contains a valid GTID
     * the commit order critical section must be entered with that GTID.
     * @param[in,out] wsrep provider handle
     * @param[in,out] conn_id connection ID
     * @param[in,out] ws_handle writeset of committing transaction
     * @param[in] flags fine tuning the replication WSREP_FLAG_*
     * @param[in,out] meta transaction meta data
     * @retval WSREP_OK writeset successfully certified, can commit
     * @retval WSREP_TRX_FAIL must rollback transaction
     * @retval WSREP_CONN_FAIL must close client connection
     * @retval WSREP_NODE_FAIL must close all connections and reinit */
    wsrep_status_t (*certify)(wsrep_t*                wsrep,
                              wsrep_conn_id_t         conn_id,
                              wsrep_ws_handle_t*      ws_handle,
                              uint32_t                flags,
                              wsrep_trx_meta_t*       meta);

    /** Enters commit order critical section.
     * Anything executed between this call and commit_order_leave() will be
     * executed in provider enforced order.
     * @param[in,out] wsrep provider handle
     * @param[in] ws_handle internal provider writeset handle
     * @param[in] meta transaction meta data
     * @retval WSREP_OK commit order entered successfully
     * @retval WSREP_NODE_FAIL must close all connections and reinit */
    wsrep_status_t (*commit_order_enter)(wsrep_t*                 wsrep,
                                         const wsrep_ws_handle_t* ws_handle,
                                         const wsrep_trx_meta_t* meta);

    /** Leaves commit order critical section.
     * Anything executed between commit_order_enter() and this call will be
     * executed in provider enforced order.
     * @param[in,out] wsrep provider handle
     * @param[in] ws_handle internal provider writeset handle
     * @param[in] meta transaction meta data
     * @param[in] error buffer containing error info (null/empty for no error)
     * @retval WSREP_OK commit order left successfully
     * @retval WSREP_NODE_FAIL must close all connections and reinit */
    wsrep_status_t (*commit_order_leave)(wsrep_t*                 wsrep,
                                         const wsrep_ws_handle_t* ws_handle,
                                         const wsrep_trx_meta_t*  meta,
                                         const wsrep_buf_t*       error);

    /** Releases resources after transaction commit/rollback.
     * Ends total order critical section.
     * @param[in,out] wsrep provider handle
     * @param[in,out] ws_handle writeset of committing transaction
     * @retval WSREP_OK  release succeeded */
    wsrep_status_t (*release) (wsrep_t*            wsrep,
                               wsrep_ws_handle_t*  ws_handle);

    /** Replay trx as a slave writeset.
     * If local trx has been aborted by brute force, and it has already
     * replicated before this abort, we must try if we can apply it as
     * slave trx. Note that slave nodes see only trx writesets and certification
     * test based on write set content can be different to DBMS lock conflicts.
     * @param[in,out] wsrep provider handle
     * @param[in,out] ws_handle writeset of committing transaction
     * @param[in,out] trx_ctx transaction context
     * @retval WSREP_OK cluster commit succeeded
     * @retval WSREP_TRX_FAIL must rollback transaction
     * @retval WSREP_BF_ABORT brute force abort happened after trx replicated
     * must rollback transaction and try to replay
     * @retval WSREP_CONN_FAIL must close client connection
     * @retval WSREP_NODE_FAIL  must close all connections and reinit */
    wsrep_status_t (*replay_trx)(wsrep_t*                  wsrep,
                                 const wsrep_ws_handle_t*  ws_handle,
                                 void*                     trx_ctx);

    /** Abort certify() call of another thread.
     * It is possible, that some high-priority transaction needs to abort
     * another transaction which is in certify() call waiting for resources.
     *
     * The kill routine checks that abort is not attempted against a transaction
     * which is front of the caller (in total order).
     *
     * If the abort was successful, the victim sequence number is stored
     * into location pointed by the victim_seqno.
     * @param[in,out] wsrep provider handle
     * @param[in,out] bf_seqno seqno of brute force trx, running this cancel
     * @param[in,out] victim_trx transaction to be aborted, and which is committing
     * @param[in,out] victim_seqno seqno of the victim transaction if assigned
     * @retval WSREP_OK abort succeeded
     * @retval WSREP_NOT_ALLOWED the provider declined the abort request
     * @retval WSREP_TRX_MISSING the victim_trx was missing
     * @retval WSREP_WARNING abort failed */
    wsrep_status_t (*abort_certification)(wsrep_t*       wsrep,
                                          wsrep_seqno_t  bf_seqno,
                                          wsrep_trx_id_t victim_trx,
                                          wsrep_seqno_t* victim_seqno);

    /** Send a rollback fragment on behalf of trx.
     * @param[in,out] wsrep provider handle
     * @param[in] trx transaction to be rolled back
     * @param[in] data data to append to the fragment
     * @retval WSREP_OK rollback fragment sent successfully */
    wsrep_status_t (*rollback)(wsrep_t*           wsrep,
                               wsrep_trx_id_t     trx,
                               const wsrep_buf_t* data);

    /** Appends a row reference to transaction writeset.
     * Both copy flag and key_type can be ignored by provider (key type
     * interpreted as WSREP_KEY_EXCLUSIVE).
     * @param[in,out] wsrep provider handle
     * @param[in] ws_handle writeset handle
     * @param[in] keys array of keys
     * @param[in] count length of the array of keys
     * @param[in] type type of the key
     * @param[in] copy can be set to FALSE if keys persist through commit. */
    wsrep_status_t (*append_key)(wsrep_t*            wsrep,
                                 wsrep_ws_handle_t*  ws_handle,
                                 const wsrep_key_t*  keys,
                                 size_t              count,
                                 enum wsrep_key_type type,
                                 wsrep_bool_t        copy);

    /** Appends data to transaction writeset.
     * This method can be called any time before commit and it
     * appends a number of data buffers to transaction writeset.
     *
     * Both copy and unordered flags can be ignored by provider.
     * @param[in,out] wsrep provider handle
     * @param[in,out] ws_handle writeset handle
     * @param[in] data array of data buffers
     * @param[in] count buffer count
     * @param[in] type type of data
     * @param[in] copy can be set to FALSE if data persists through commit. */
    wsrep_status_t (*append_data)(wsrep_t*             wsrep,
                                  wsrep_ws_handle_t*   ws_handle,
                                  const wsrep_buf_t*   data,
                                  size_t               count,
                                  enum wsrep_data_type type,
                                  wsrep_bool_t         copy);

    /** Blocks until the given GTID is committed.
     * This call will block the caller until the given GTID
     * is guaranteed to be committed, or until a timeout occurs.
     * The timeout value is given in parameter tout, if tout is -1,
     * then the global causal read timeout applies.
     *
     * If no pointer upto is provided the call will block until
     * causal ordering with all possible preceding writes in the
     * cluster is guaranteed.
     *
     * If pointer to gtid is non-null, the call stores the global
     * transaction ID of the last transaction which is guaranteed
     * to be committed when the call returns.
     * @param[in,out] wsrep provider handle
     * @param[in,out] upto gtid to wait upto
     * @param[in] tout timeout in seconds, -1 wait for global causal read timeout
     * @param[in,out] gtid location to store GTID */
    wsrep_status_t (*sync_wait)(wsrep_t*      wsrep,
                                wsrep_gtid_t* upto,
                                int           tout,
                                wsrep_gtid_t* gtid);

    /** Returns the last committed gtid.
     * @param[in,out] wsrep provider handle
     * @param[in,out] gtid location to store GTID */
    wsrep_status_t (*last_committed_id)(wsrep_t*      wsrep,
                                        wsrep_gtid_t* gtid);

    /** Clears allocated connection context.
     * Whenever a new connection ID is passed to wsrep provider through
     * any of the API calls, a connection context is allocated for this
     * connection. This call is to explicitly notify provider of connection
     * closing.
     * @param[in,out] wsrep provider handle
     * @param[in] conn_id connection ID */
    wsrep_status_t (*free_connection)(wsrep_t*        wsrep,
                                      wsrep_conn_id_t conn_id);

    /** Replicates a query and starts "total order isolation" section.
     *
     * Regular mode:
     *
     * Replicates the action spec and returns success code, which caller must
     * check. Total order isolation continues until to_execute_end() is called.
     * Regular "total order isolation" is achieved by calling to_execute_start()
     * with WSREP_FLAG_TRX_START and WSREP_FLAG_TRX_END set.
     *
     * Two-phase mode:
     *
     * In this mode a query execution is split in two phases. The first phase is
     * acquiring total order isolation to access critical section and the
     * second phase is to release acquired resources in total order.
     *
     * To start the first phase the call is made with WSREP_FLAG_TRX_START set.
     * The action is replicated and success code is returned. The total order
     * isolation continues until to_execute_end() is called. However, the provider
     * will keep the reference to the operation for conflict resolution purposes.
     *
     * The second phase is started with WSREP_FLAG_TRX_END set. Provider
     * returns once it has achieved total ordering isolation for second phase.
     * Total order isolation continues until to_execute_end() is called.
     * All references to the operation are cleared by provider before
     * call to to_execute_end() returns.
     * @param[in,out] wsrep provider handle
     * @param[in] conn_id connection ID
     * @param[in] keys array of keys
     * @param[in] keys_num length of the array of keys
     * @param[in] action action buffer array to be executed
     * @param[in] count action buffer count
     * @param[in] flags flags
     * @param[in] meta transaction meta data
     * @retval WSREP_OK cluster commit succeeded
     * @retval WSREP_CONN_FAIL must close client connection
     * @retval WSREP_NODE_FAIL must close all connections and reinit */
    wsrep_status_t (*to_execute_start)(wsrep_t*           wsrep,
                                       wsrep_conn_id_t    conn_id,
                                       const wsrep_key_t* keys,
                                       size_t             keys_num,
                                       const wsrep_buf_t* action,
                                       size_t             count,
                                       uint32_t           flags,
                                       wsrep_trx_meta_t*  meta);

    /** Ends the total order isolation section.
     * Marks the end of total order isolation. TO locks are freed
     * and other transactions are free to commit from this point on.
     * @param[in,out] wsrep provider handle
     * @param[in] conn_id connection ID
     * @param[in] error error information about TOI operation (empty for no error)
     * @retval WSREP_OK cluster commit succeeded
     * @retval WSREP_CONN_FAIL must close client connection
     * @retval WSREP_NODE_FAIL must close all connections and reinit */
    wsrep_status_t (*to_execute_end)(wsrep_t*           wsrep,
                                     wsrep_conn_id_t    conn_id,
                                     const wsrep_buf_t* error);


    /** Collects preordered replication events into a writeset.
     * @param[in,out] wsrep wsrep provider handle
     * @param[in,out] handle a handle associated with a given writeset
     * @param[in] data an array of data buffers.
     * @param[in] count length of data buffer array.
     * @param[in] copy whether provider needs to make a copy of events.
     * @retval WSREP_OK cluster-wide commit succeeded
     * @retval WSREP_TRX_FAIL operation failed (e.g. trx size exceeded limit)
     * @retval WSREP_NODE_FAIL must close all connections and reinit */
    wsrep_status_t (*preordered_collect) (wsrep_t*           wsrep,
                                          wsrep_po_handle_t* handle,
                                          const wsrep_buf_t* data,
                                          size_t             count,
                                          wsrep_bool_t       copy);

    /** "Commits" preordered writeset to cluster.
     * The contract is that the writeset will be committed in the same (partial)
     * order this method was called. Frees resources associated with the
     * writeset handle and reinitializes the handle.
     * @param[in,out] wsrep wsrep provider handle
     * @param[in,out] handle a handle associated with a given writeset
     * @param[in] source_id ID of the event producer, also serves as the
     * partial order or stream ID - events with different source_ids won't be
     * ordered with respect to each other.
     * @param[in] flags WSREP_FLAG_... flags
     * @param[in] pa_range the number of preceding events this event can be
     * processed in parallel with. A value of 0 means strict serial processing.
     * Note: commits always happen in wsrep order.
     * @param[in] commit 'true' to commit writeset to cluster (replicate) or
     * 'false' to rollback (cancel) the writeset.
     * @retval WSREP_OK cluster-wide commit succeeded
     * @retval WSREP_TRX_FAIL operation failed (e.g. NON-PRIMARY component)
     * @retval WSREP_NODE_FAIL must close all connections and reinit */
    wsrep_status_t (*preordered_commit)  (wsrep_t*             wsrep,
                                          wsrep_po_handle_t*   handle,
                                          const wsrep_uuid_t*  source_id,
                                          uint32_t             flags,
                                          int                  pa_range,
                                          wsrep_bool_t         commit);

    /** Signals to wsrep provider that state snapshot has been sent to joiner.
     * @param[in,out] wsrep provider handle
     * @param[in] state_id state ID
     * @param[in] rcode 0 or negative error code of the operation */
    wsrep_status_t (*sst_sent)(wsrep_t*            wsrep,
                               const wsrep_gtid_t* state_id,
                               int                 rcode);

    /** Signals to wsrep provider that new state snapshot has been received.
     * May deadlock if called from sst_prepare_cb.
     * @param[in,out] wsrep provider handle
     * @param[in] state_id state ID
     * @param[in] state initial state provided by SST donor
     * @param[in] rcode 0 or negative error code of the operation. */
    wsrep_status_t (*sst_received)(wsrep_t*            wsrep,
                                   const wsrep_gtid_t* state_id,
                                   const wsrep_buf_t*  state,
                                   int                 rcode);


    /** Generate request for consistent snapshot.
     * If successful, this call will generate internally SST request
     * which in turn triggers calling SST donate callback on the nodes
     * specified in donor_spec. If donor_spec is null, callback is
     * called only locally. This call will block until sst_sent is called
     * from callback.
     * @param[in,out] wsrep provider handle
     * @param[in] msg context message for SST donate callback
     * @param[in] donor_spec list of snapshot donors */
    wsrep_status_t (*snapshot)(wsrep_t*           wsrep,
                               const wsrep_buf_t* msg,
                               const char*        donor_spec);

    /** Returns an array of status variables.
     * Array is terminated by Null variable name.
     * @param[in] wsrep provider handle
     * @return array of struct wsrep_status_var */
    struct wsrep_stats_var* (*stats_get) (wsrep_t* wsrep);

    /** Release resources that might be associated with the array.
     * @param[in,out] wsrep provider handle.
     * @param[in,out] var_array array returned by stats_get().
     */
    void (*stats_free) (wsrep_t* wsrep, struct wsrep_stats_var* var_array);

    /** Reset some stats variables to inital value, provider-dependent.
     * @param[in,out] wsrep provider handle */
    void (*stats_reset) (wsrep_t* wsrep);

    /** Pauses writeset applying/committing.
     * @param[in,out] wsrep provider handle
     * @return global sequence number of the paused state or negative error
     * code. */
    wsrep_seqno_t (*pause) (wsrep_t* wsrep);

    /** Resumes writeset applying/committing.
     * @param[in,out] wsrep provider handle */
    wsrep_status_t (*resume) (wsrep_t* wsrep);

    /** Desynchronize from cluster.
     * Effectively turns off flow control for this node, allowing it
     * to fall behind the cluster.
     * @param[in,out] wsrep provider handle */
    wsrep_status_t (*desync) (wsrep_t* wsrep);

    /** Request to resynchronize with cluster.
     * Effectively turns on flow control. Asynchronous - actual synchronization
     * event to be delivered via sync_cb.
     * @param[in,out] wsrep provider handle */
    wsrep_status_t (*resync) (wsrep_t* wsrep);

    /** Acquire global named lock.
     * @param[in,out] wsrep wsrep provider handle
     * @param[in] name lock name
     * @param[in] shared shared or exclusive lock
     * @param[in] owner 64-bit owner ID
     * @param[in] tout timeout in nanoseconds. 0 - return immediately,
     * -1 wait forever.
     * @return wsrep status or negative error code
     * @retval -EDEADLK lock was already acquired by this thread
     * @retval -EBUSY lock was busy */
    wsrep_status_t (*lock) (wsrep_t* wsrep,
                            const char* name, wsrep_bool_t shared,
                            uint64_t owner, int64_t tout);

    /** Release global named lock.
     * @param[in,out] wsrep wsrep provider handle
     * @param[in] name lock name
     * @param[in] owner 64-bit owner ID
     * @return wsrep status or negative error code
     * @retval -EPERM lock does not belong to this owner */
    wsrep_status_t (*unlock) (wsrep_t* wsrep, const char* name, uint64_t owner);

    /** Check if global named lock is locked.
     * @param[in,out] wsrep wsrep provider handle
     * @param[in] name lock name
     * @param[in] owner if not NULL will contain 64-bit owner ID
     * @param[in] node if not NULL will contain owner's node UUID
     * @return true if lock is locked */
    wsrep_bool_t (*is_locked) (wsrep_t* wsrep, const char* name, uint64_t* owner,
                               wsrep_uuid_t* node);

    /** WSREP provider name. */
    const char* provider_name;

    /** WSREP provider version. */
    const char* provider_version;

    /** WSREP provider vendor name. */
    const char* provider_vendor;

    /** Frees allocated resources before unloading the library.
     * @param[in,out] wsrep provider handle */
    void (*free)(wsrep_t* wsrep);

    /** Reserved for future use. */
    void *dlh;

    /** Reserved for implementation private context. */
    void *ctx;
};


/** Loads wsrep library.
 * @param[in] spec path to wsrep library. If NULL or WSREP_NONE initializes
 * dummy pass-through implementation.
 * @param[out] hptr wsrep handle
 * @param[in,out] log_cb callback to handle loader messages. Otherwise writes
 * to stderr.
 * @return zero on success, errno on failure
 */
int wsrep_load(const char* spec, wsrep_t** hptr, wsrep_log_cb_t log_cb);

/** Unloads wsrep library and frees associated resources.
 * @param[in,out] hptr wsrep handler pointer */
void wsrep_unload(wsrep_t* hptr);

#ifdef __cplusplus
}
#endif

#endif /* WSREP_H */
