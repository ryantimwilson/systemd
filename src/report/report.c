/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <getopt.h>

#include "build.h"
#include "log.h"
#include "main-func.h"
#include "metrics.h"

static char* arg_pattern = NULL;

STATIC_DESTRUCTOR_REGISTER(arg_pattern, freep);

static int help(void) {
        /* TODO: Support multiple patterns. */
        printf("%s [OPTIONS...]\n\n"
               "Print metrics for all systemd components .\n\n"
               "  -h --help             Show this help\n"
               "     --version          Show package version\n"
               "  -p --pattern          Only return metrics matching this glob pattern\n",
               program_invocation_short_name);

        return 0;
}

static int parse_argv(int argc, char *argv[]) {

        enum {
                ARG_VERSION = 0x100
        };

        static const struct option options[] = {
                { "help",          no_argument,       NULL, 'h'               },
                { "version",       no_argument,       NULL, ARG_VERSION       },
                { "pattern",       required_argument, NULL, 'p'               },
                {}
        };

        int c;

        assert(argc >= 0);
        assert(argv);

        while ((c = getopt_long(argc, argv, "hp", options, NULL)) >= 0)

                switch (c) {

                case 'h':
                        return help();

                case ARG_VERSION:
                        return version();

                case 'p':
                        if (isempty(optarg))
                                arg_pattern = mfree(arg_pattern);
                        else {
                                arg_pattern = strdup(optarg);
                                if (!arg_pattern)
                                        return log_oom();
                        }
                        break;

                case '?':
                        return -EINVAL;

                default:
                        assert_not_reached();
                }

        if (optind < argc)
                return log_error_errno(SYNTHETIC_ERRNO(EINVAL),
                                       "%s takes no arguments.",
                                       program_invocation_short_name);

        return 1;
}

static int run(int argc, char *argv[]) {
        _cleanup_(sd_json_variant_unrefp) sd_json_variant *ret = NULL;
        int r;

        /* This is mostly intended to be used for scripts which want
         * to detect whether we are being run in a virtualized
         * environment or not */

        log_setup();

        r = parse_argv(argc, argv);
        if (r <= 0)
                return r;


        r = metrics_query_all(&ret, arg_pattern);
        if (r < 0)
                return r;

        return sd_json_variant_dump(ret, SD_JSON_FORMAT_PRETTY_AUTO, stdout, NULL);
}

DEFINE_MAIN_FUNCTION_WITH_POSITIVE_FAILURE(run);
