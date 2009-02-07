/* Copyright (C) 2009 Codership Oy <info@codersihp.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
 
#include <dlfcn.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>

#include "wsrep.h"

// Logging stuff for the loader
static const char* log_levels[] = {"FATAL", "ERROR", "WARN", "INFO", "DEBUG"};

static void default_logger (wsrep_log_level_t lvl, const char* msg)
{
    fprintf (stderr, "wsrep loader: [%s] %s\n", log_levels[lvl], msg);
}

static wsrep_log_cb_t logger = default_logger;

struct dummy_wsrep
{
    wsrep_log_cb_t log_fn;
};

/* Get pointer to dummy_wsrep from wsrep_t pointer */
#define DUMMY_PRIV(_p) ((struct dummy_wsrep *) (_p)->opaque)

#define DBUG_ENTER(_w) do {                                             \
        if (DUMMY_PRIV(_w)->log_fn)                                     \
            DUMMY_PRIV(_w)->log_fn(WSREP_LOG_DEBUG, __FUNCTION__);      \
    } while (0)


static void dummy_tear_down(wsrep_t *w)
{
    DBUG_ENTER(w);
    free(w->opaque);
    w->opaque = NULL;
}

static wsrep_status_t dummy_init(wsrep_t *w, const wsrep_init_args_t* args)
{
    DUMMY_PRIV(w)->log_fn = args->logger_cb;
    DBUG_ENTER(w);
    return WSREP_OK;
}

static wsrep_status_t dummy_enable(wsrep_t *w)
{
    DBUG_ENTER(w);
    return WSREP_OK;
}

static wsrep_status_t dummy_disable(wsrep_t *w)
{
    DBUG_ENTER(w);
    return WSREP_OK;
}

static wsrep_status_t dummy_recv(wsrep_t *w, void *ctx __attribute__((unused)))
{
    DBUG_ENTER(w);
    return WSREP_OK;
}

static void dummy_dbug_push(wsrep_t *w,
                            const char *ctrl __attribute__((unused)))
{
    DBUG_ENTER(w);
}

static void dummy_dbug_pop(wsrep_t *w)
{
    DBUG_ENTER(w);
}

static wsrep_status_t dummy_commit(
    wsrep_t *w, 
    const trx_id_t   trx_id    __attribute__((unused)), 
    const conn_id_t  conn_id   __attribute__((unused)), 
    const char      *query     __attribute__((unused)), 
    const size_t     query_len __attribute__((unused)))
{
    DBUG_ENTER(w);
    return WSREP_OK;
}

static wsrep_status_t dummy_replay_trx(
    wsrep_t *w, 
    const trx_id_t  trx_id  __attribute__((unused)), 
    void           *app_ctx __attribute__((unused)))
{
    DBUG_ENTER(w);
    return WSREP_OK;
}

static wsrep_status_t dummy_cancel_commit(
    wsrep_t *w, 
    const wsrep_seqno_t bf_seqno __attribute__((unused)), 
    const trx_id_t      trx_id   __attribute__((unused)))
{
    DBUG_ENTER(w);
    return WSREP_OK;
}

static wsrep_status_t dummy_cancel_slave(
    wsrep_t *w, 
    const wsrep_seqno_t bf_seqno     __attribute__((unused)), 
    const wsrep_seqno_t victim_seqno __attribute__((unused)))
{
    DBUG_ENTER(w);
    return WSREP_OK;
}

static wsrep_status_t dummy_committed(
    wsrep_t *w, 
    const trx_id_t trx_id __attribute__((unused)))
{
    DBUG_ENTER(w);
    return WSREP_OK;
}

static wsrep_status_t dummy_rolledback(
    wsrep_t *w, 
    const trx_id_t trx_id __attribute__((unused)))
{
    DBUG_ENTER(w);
    return WSREP_OK;
}


static wsrep_status_t dummy_append_query(
    wsrep_t *w, 
    const trx_id_t  trx_id   __attribute__((unused)), 
    const char     *query    __attribute__((unused)), 
    const time_t    timeval  __attribute__((unused)),
    const uint32_t  randseed __attribute__((unused)))
{
    DBUG_ENTER(w);
    return WSREP_OK;
}

static wsrep_status_t dummy_append_row_key(
    wsrep_t *w, 
    const trx_id_t       trx_id      __attribute__((unused)), 
    const char          *dbtable     __attribute__((unused)),
    const size_t         dbtable_len __attribute__((unused)),
    const char          *key         __attribute__((unused)), 
    const size_t         key_len     __attribute__((unused)), 
    const wsrep_action_t action      __attribute__((unused)))
{
    DBUG_ENTER(w);
    return WSREP_OK;
}


static wsrep_status_t dummy_set_variable(
    wsrep_t *w, 
    const conn_id_t  conn_id   __attribute__((unused)), 
    const char      *key       __attribute__((unused)), 
    const size_t     key_len   __attribute__((unused)),
    const char      *query     __attribute__((unused)), 
    const size_t     query_len __attribute__((unused)))
{
    DBUG_ENTER(w);
    return WSREP_OK;
}

static wsrep_status_t dummy_set_database(
    wsrep_t *w, 
    const conn_id_t  conn_id   __attribute__((unused)), 
    const char      *query     __attribute__((unused)), 
    const size_t     query_len __attribute__((unused)))
{
    DBUG_ENTER(w);
    return WSREP_OK;
}

static wsrep_status_t dummy_to_execute_start(
    wsrep_t *w, 
    const conn_id_t  conn_id   __attribute__((unused)),
    const char      *query     __attribute__((unused)), 
    const size_t     query_len __attribute__((unused)))
{
    DBUG_ENTER(w);
    return WSREP_OK;
}

static wsrep_status_t dummy_to_execute_end(
    wsrep_t *w,
    const conn_id_t  conn_id   __attribute__((unused)))
{
    DBUG_ENTER(w);
    return WSREP_OK;
}

static wsrep_t dummy_init_str = {
    WSREP_INTERFACE_VERSION,
    &dummy_init,
    &dummy_enable,
    &dummy_disable,
    &dummy_dbug_push,
    &dummy_dbug_pop,
    &dummy_recv,
    &dummy_commit,
    &dummy_replay_trx,
    &dummy_cancel_commit,
    &dummy_cancel_slave,
    &dummy_committed,
    &dummy_rolledback,
    &dummy_append_query,
    &dummy_append_row_key,
    &dummy_set_variable,
    &dummy_set_database,
    &dummy_to_execute_start,
    &dummy_to_execute_end,
    &dummy_tear_down,
    NULL,
    NULL
};

static int dummy_loader(wsrep_t *hptr)
{
    if (!hptr)
        return EINVAL;
    
    *hptr = dummy_init_str;
    
    if (!(hptr->opaque = malloc(sizeof(struct dummy_wsrep))))
        return ENOMEM;
    
    DUMMY_PRIV(hptr)->log_fn = NULL;
    
    return 0;
}




/**************************************************************************
 * Library loader
 **************************************************************************/

static int verify(const wsrep_t *wh, const char *iface_ver)
{
    const size_t msg_len = 128;
    char msg[msg_len];

#define VERIFY(_p) if (!(_p)) {                                       \
	snprintf(msg, msg_len, "wsrep_load(): verify(): %s\n", # _p); \
        logger (WSREP_LOG_ERROR, msg);                                \
	return EINVAL;						      \
    }

    VERIFY(wh);
    VERIFY(wh->version);
    VERIFY(strcmp(wh->version, iface_ver) == 0);
    VERIFY(wh->init);
    VERIFY(wh->enable);
    VERIFY(wh->disable);
    VERIFY(wh->dbug_push);
    VERIFY(wh->dbug_pop);
    VERIFY(wh->recv);
    VERIFY(wh->commit);
    VERIFY(wh->replay_trx);
    VERIFY(wh->cancel_commit);
    VERIFY(wh->cancel_slave);
    VERIFY(wh->committed);
    VERIFY(wh->rolledback);
    VERIFY(wh->append_query);
    VERIFY(wh->append_row_key);
    VERIFY(wh->set_variable);
    VERIFY(wh->set_database);
    VERIFY(wh->to_execute_start);
    VERIFY(wh->to_execute_end);
    return 0;
}


static wsrep_loader_fun wsrep_dlf(void *dlh, const char *sym)
{
    union {
	wsrep_loader_fun dlfun;
	void *obj;
    } alias;
    alias.obj = dlsym(dlh, sym);
    return alias.dlfun;
}

int wsrep_load(const char *spec, wsrep_t **hptr, wsrep_log_cb_t log_cb)
{
    int ret = 0;
    void *dlh = NULL;
    wsrep_loader_fun dlfun;
    const size_t msg_len = 128;
    char msg[msg_len];

    if (NULL != log_cb)
        logger = log_cb;
    
    if (!(spec && hptr))
        return EINVAL;
    
    snprintf (msg, msg_len,
              "wsrep_load(): loading provider library '%s'\n", spec);
    logger (WSREP_LOG_INFO, msg);
    
    if (!(*hptr = malloc(sizeof(wsrep_t)))) {
	logger (WSREP_LOG_FATAL, "wsrep_load(): out of memory");
        return ENOMEM;
    }

    if (strcmp(spec, WSREP_NONE) == 0) {
        if ((ret = dummy_loader(*hptr)) != 0) {
	    free (*hptr);
            *hptr = NULL;
        }
	return ret;
    }
    
    if (!(dlh = dlopen(spec, RTLD_NOW | RTLD_LOCAL))) {
	snprintf(msg, msg_len, "wsrep_load(): dlopen(): %s\n", dlerror());
        logger (WSREP_LOG_ERROR, msg);
        ret = EINVAL;
	goto out;
    }
    
    if (!(dlfun = wsrep_dlf(dlh, "wsrep_loader"))) {
        ret = EINVAL;
	goto out;
    }
    
    if ((ret = (*dlfun)(*hptr)) != 0) {
        snprintf(msg, msg_len, "wsrep_load(): loader failed: %s\n",
                 strerror(ret));
        logger (WSREP_LOG_ERROR, msg);
        goto out;
    }
    
    if ((ret = verify(*hptr, WSREP_INTERFACE_VERSION)) != 0 &&
        (*hptr)->tear_down) {
	logger (WSREP_LOG_ERROR, "wsrep_load(): interface version mismatch\n");
        (*hptr)->tear_down(*hptr);
        goto out;
    }
    
    (*hptr)->dlh = dlh;

out:
    if (ret != 0) {
        if (dlh)
            dlclose(dlh);
        free(*hptr);
        *hptr = NULL;
    } else {
        logger (WSREP_LOG_INFO, "wsrep_load(): provider loaded succesfully\n");
    }

    return ret;
}



void wsrep_unload(wsrep_t *hptr)
{
    if (!hptr) {
        logger (WSREP_LOG_WARN, "wsrep_unload(): null pointer\n");
    } else {
        if (hptr->dlh)
            dlclose(hptr->dlh);
        free(hptr);
    }
}
