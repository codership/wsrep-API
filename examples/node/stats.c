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

#include "stats.h"

#include "log.h"

#include <assert.h>
#include <stdio.h>  // snprintf()
#include <stdlib.h> // abort()
#include <string.h> // strcmp()
#include <unistd.h> // usleep()

enum
{
    _STATS_REPL_BYTE,
    _STATS_REPL_WS,
    _STATS_RECV_BYTE,
    _STATS_RECV_WS,
    _STATS_TOTAL_BYTE,
    _STATS_TOTAL_WS,
    _STATS_CERT_FAILS,
    _STATS_FC_PAUSED,
    _STATS_MAX
};

static const char* const _stats_str[_STATS_MAX] =
{
    " repl(B/s)",
    " repl(W/s)",
    " recv(B/s)",
    " recv(W/s)",
    "total(B/s)",
    "total(W/s)",
    " cert.fail",
    " paused(%)"
};

/* stats IDs in provider output - provider dependent, here we use Galera's */
static const char* const _galera_ids[_STATS_MAX] =
{
    "replicated_bytes",       /**<  _STATS_REPL_BYTE  */
    "replicated",             /**<  _STATS_REPL_WS    */
    "received_bytes",         /**<  _STATS_RECV_BYTE  */
    "received",               /**<  _STATS_RECV_WS    */
    "",                       /**<  _STATS_TOTAL_BYTE */
    "",                       /**<  _STATS_TOTAL_WS   */
    "local_cert_failures",    /**<  _STATS_CERT_FAILS */
    "flow_control_paused_ns"  /**<  _STATS_FC_PAUSED  */
};

/* maps local stats IDs to provider stat IDs */
static int _stats_galera_map[_STATS_MAX];

/**
 * Helper to map provider stats to own stats set */
static void
_stats_establish_mapping(wsrep_t* const wsrep)
{
    int const magic_map = -1;
    size_t i;
    for (i = 0; i < sizeof(_stats_galera_map)/sizeof(_stats_galera_map[0]); i++)
    {
        _stats_galera_map[i] = magic_map; /* initialize map array */
    }

    struct wsrep_stats_var* const ret = wsrep->stats_get(wsrep);

    int mapped = 2; /* to compensate for _STATS_TOTAL_* having no counterparts */
    i = 0;
    while (ret[i].name)
    {
        int j;
        for (j = 0; j < _STATS_MAX; j++)
        {
            if (magic_map == _stats_galera_map[j] /* j-th member still unset */
                &&
                !strcmp(ret[i].name, _galera_ids[j]))
            {
                _stats_galera_map[j] = (int)i;
                mapped++;
                if (_STATS_MAX == mapped) /* all mapped */ return;
            }
        }

        i++;
    }
}

static void
_stats_get(wsrep_t* const wsrep, long long stats[])
{
    struct wsrep_stats_var* const ret = wsrep->stats_get(wsrep);
    if (!ret)
    {
        NODE_FATAL("wsrep::stats_get() call failed.");
        abort();
    }

    int i;
    for (i = 0; i < _STATS_MAX; i++)
    {
        int j = _stats_galera_map[i];
        if (j >= 0)
        {
            assert(WSREP_VAR_INT64 == ret[j].type);
            stats[i] = ret[j].value._int64;
        }
    }

    wsrep->stats_free(wsrep, ret);

    // totals are just sums
    stats[_STATS_TOTAL_BYTE] = stats[_STATS_REPL_BYTE] + stats[_STATS_RECV_BYTE];
    stats[_STATS_TOTAL_WS  ] = stats[_STATS_REPL_WS  ] + stats[_STATS_RECV_WS  ];
}

static void
_stats_print(long long bef[], long long aft[], double period)
{
    double rate[_STATS_MAX];
    int i;
    for (i = 0; i < _STATS_MAX; i++)
    {
        rate[i] = (double)(aft[i] - bef[i])/period;
    }
    rate[_STATS_FC_PAUSED] /= 1.0e+07; // convert from nanosecons to % of seconds

    char   str[256];
    int    written = 0;

    /* first line write legend */
    for (i = 0; i < _STATS_MAX; i++)
    {
        size_t const space_left = sizeof(str) - (size_t)written;
        written += snprintf(&str[written], space_left, "%s", _stats_str[i]);
    }

    str[written] = '\n';
    written++;

    /* second line write values */
    for (i = 0; i < _STATS_MAX; i++)
    {
        size_t const space_left = sizeof(str) - (size_t)written;
        long long const value   = (long long)rate[i];
        written += snprintf(&str[written], space_left, " %9lld", value);
    }

    str[written] = '\0';

    /* use logging macro for timestamp */
    NODE_INFO("\n%s", str);
}

void
node_stats_loop(wsrep_t* const wsrep, int const period)
{
    double     const period_sec  = period;
    useconds_t const period_usec = (useconds_t)period * 1000000;

    _stats_establish_mapping(wsrep);

    long long stats1[_STATS_MAX];
    long long stats2[_STATS_MAX];

    _stats_get(wsrep, stats1);

    while (1)
    {
        usleep(period_usec);
        _stats_get(wsrep, stats2);
        _stats_print(stats1, stats2, period_sec);

        usleep(period_usec);
        _stats_get(wsrep, stats1);
        _stats_print(stats2, stats1, period_sec);
    }
}
