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

#include "options.h"

#include <ctype.h>  // isspace()
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h> // strtol()
#include <string.h> // strcmp()

/*
 * getopt_long() declarations begin
 */

#define NODE_NA no_argument
#define NODE_RA required_argument
#define NODE_OA optional_argument

typedef enum opt
{
    NODE_OPT_NOOPT     = 0,
    NODE_OPT_ADDRESS   = 'a',
    NODE_OPT_BOOTSTRAP = 'b',
    NODE_OPT_DELAY     = 'd',
    NODE_OPT_DATA_DIR  = 'f',
    NODE_OPT_HELP      = 'h',
    NODE_OPT_PERIOD    = 'i',
    NODE_OPT_MASTERS   = 'm',
    NODE_OPT_NAME      = 'n',
    NODE_OPT_OPTIONS   = 'o',
    NODE_OPT_BASE_PORT = 'p',
    NODE_OPT_RECORDS   = 'r',
    NODE_OPT_SLAVES    = 's',
    NODE_OPT_BASE_HOST = 't',
    NODE_OPT_PROVIDER  = 'v',
    NODE_OPT_REC_SIZE  = 'w'
}
      opt_t;

static struct option _opts[] =
{
    { "address",   NODE_RA, NULL, NODE_OPT_ADDRESS   },
    { "bootstrap", NODE_NA, NULL, NODE_OPT_BOOTSTRAP },
    { "delay",     NODE_RA, NULL, NODE_OPT_DELAY     },
    { "storage",   NODE_RA, NULL, NODE_OPT_DATA_DIR  },
    { "help",      NODE_NA, NULL, NODE_OPT_HELP      },
    { "period",    NODE_RA, NULL, NODE_OPT_PERIOD    },
    { "masters",   NODE_RA, NULL, NODE_OPT_MASTERS   },
    { "name",      NODE_RA, NULL, NODE_OPT_NAME      },
    { "options",   NODE_RA, NULL, NODE_OPT_OPTIONS,  },
    { "base-port", NODE_RA, NULL, NODE_OPT_BASE_PORT },
    { "records",   NODE_RA, NULL, NODE_OPT_RECORDS   },
    { "slaves",    NODE_RA, NULL, NODE_OPT_SLAVES    },
    { "base-host", NODE_RA, NULL, NODE_OPT_BASE_HOST },
    { "provider",  NODE_RA, NULL, NODE_OPT_PROVIDER  },
    { "size",      NODE_RA, NULL, NODE_OPT_REC_SIZE  },
    { NULL, 0, NULL, 0 }
};

static const char* _optstring = "a:d:f:hi:m:n:o:p:r:s:t:v:w:";

/*
 * getopt_long() declarations end
 */

static const struct node_options _defaults =
{
    .provider  = "none",
    .address   = "gcomm://",
    .options   = "",
    .name      = "unnamed",
    .data_dir  = ".",
    .base_host = "localhost",
    .masters   = 0,
    .slaves    = 1,
    .rec_size  = 1024,
    .records   = 0,
    .delay     = 0,
    .base_port = 4567,
    .period    = 10,
    .bootstrap = true
};

static int
_check_conversion(int cond, const char* ptr, int idx)
{
    if (!cond || errno || (*ptr != '\0' && !isspace(*ptr)))
    {
        fprintf(stderr, "Bad value for %s option.\n", _opts[idx].name);
        return EINVAL;
    }
    return 0;
}

static void
_print_help(FILE* out, const char* prog_name)
{
    fprintf(
        out,
        "Usage: %s [OPTION...]\n"
        "\n"
        "  -h, --help                 this thing.\n"
        "  -v, --provider=PATH        a path to wsrep provider library file.\n"
        "  -a, --address=STRING       list of node addresses in the group.\n"
        "                             If not set the node assumes that it is the first\n"
        "                             node in the group (default)\n"
        "  -o, --options=STRING       a string of wsrep provider options.\n"
        "  -n, --name=STRING          human-readable node name.\n"
        "  -f, --data-dir=PATH        a directory to save working data in.\n"
        "                             Should be private to the process.\n"
        "  -t, --base-host=ADDRESS    address of this node at which other members can\n"
        "                             connect to it\n"
        "  -p, --base-port=NUM        base port which the node shall listen for\n"
        "                             connections from other members. This port will be\n"
        "                             used for replication, port-1 for IST and port-2\n"
        "                             for SST. Default: 4567\n"
        "  -m, --masters=NUM          number of concurrent master workers.\n"
        "  -s, --slaves=NUM           number of concurrent slave workers.\n"
        "                             (can't be less than 1)\n"
        "  -w, --size=NUM             size of a record in the store. Default: 1K\n"
        "  -r, --records=NUM          number of records in the store.\n"
        "  -d, --delay=NUM            delay in milliseconds between \"commits\"\n"
        "                             (per master thread).\n"
        "  -b, --bootstrap            bootstrap the cluster with this node.\n"
        "                             Default: 'Yes' if --address is not given, 'No'\n"
        "                             otherwise.\n"
        "  -i, --period               period in seconds between performance stats output\n"
        , prog_name);
}

static void
_print_config(FILE* out, const struct node_options* opts)
{
    fprintf(
        out,
        "Continuing with the following configuration:\n"
        "provider:      %s\n"
        "address:       %s\n"
        "options:       %s\n"
        "name:          %s\n"
        "data dir:      %s\n"
        "base addr:     %s:%ld\n"
        "masters:       %ld\n"
        "slaves:        %ld\n"
        "record size:   %ld bytes\n"
        "records:       %ld\n"
        "commit delay:  %ld ms\n"
        "stats period:  %ld s\n"
        "bootstrap:     %s\n"
        ,
        opts->provider, opts->address, opts->options, opts->name, opts->data_dir,
        opts->base_host, opts->base_port,
        opts->masters, opts->slaves, opts->rec_size, opts->records, opts->delay,
        opts->period, opts->bootstrap ? "Yes" : "No"
        );
}

int
node_options_read(int argc, char* argv[], struct node_options* opts)
{
    *opts = _defaults;

    int   opt = 0;
    int   opt_idx = 0;
    char* endptr;
    int   ret = 0;

    bool address_given   = false;
    bool bootstrap_given = false;

    while ((opt = getopt_long(argc, argv, _optstring, _opts, &opt_idx)) != -1)
    {
        switch (opt)
        {
        case NODE_OPT_ADDRESS:
            address_given = strcmp(opts->address, optarg);
            opts->address = optarg;
            break;
        case NODE_OPT_BOOTSTRAP:
            bootstrap_given = true;
            opts->bootstrap = true;
            break;
        case NODE_OPT_DELAY:
            opts->delay = strtol(optarg, &endptr, 10);
            if ((ret = _check_conversion(opts->delay >= 0, endptr, opt_idx)))
                goto err;
            break;
        case NODE_OPT_DATA_DIR:
            opts->data_dir = optarg;
            break;
        case NODE_OPT_HELP:
            ret = 1;
            goto help;
        case NODE_OPT_PERIOD:
            opts->period = strtol(optarg, &endptr, 10);
            if ((ret = _check_conversion(opts->period > 0, endptr, opt_idx)))
                goto err;
            break;
        case NODE_OPT_MASTERS:
            opts->masters = strtol(optarg, &endptr, 10);
            if ((ret = _check_conversion(opts->masters >= 0, endptr, opt_idx)))
                goto err;
            break;
        case NODE_OPT_NAME:
            opts->name = optarg;
            break;
        case NODE_OPT_OPTIONS:
            opts->options = optarg;
            break;
        case NODE_OPT_BASE_PORT:
            opts->base_port = strtol(optarg, &endptr, 10);
            if ((ret = _check_conversion(
                     opts->base_port > 0 && opts->base_port < 65536,
                     endptr, opt_idx)))
                goto err;
            break;
        case NODE_OPT_RECORDS:
            opts->records = strtol(optarg, &endptr, 10);
            if ((ret = _check_conversion(opts->records >= 0, endptr, opt_idx)))
                goto err;
            break;
        case NODE_OPT_SLAVES:
            opts->slaves = strtol(optarg, &endptr, 10);
            if ((ret = _check_conversion(opts->slaves > 0, endptr, opt_idx)))
                goto err;
            break;
        case NODE_OPT_BASE_HOST:
            opts->base_host = optarg;
            break;
        case NODE_OPT_PROVIDER:
            opts->provider = optarg;
            break;
        case NODE_OPT_REC_SIZE:
            opts->rec_size = strtol(optarg, &endptr, 10);
            if ((ret = _check_conversion(opts->rec_size > 0, endptr, opt_idx)))
                goto err;
            break;
        default:
            ret = EINVAL;
        }
    }

help:
    if (ret) {
        _print_help(stderr, argv[0]);
    }
    else
    {
        if (!bootstrap_given)
        {
            opts->bootstrap = !address_given;
        }
        _print_config(stdout, opts);
        opts->delay  *= 1000;    /* convert to microseconds for usleep() */
    }

err:
    return ret;
}
