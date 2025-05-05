/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "log.h"
#include "main-func.h"
#include "metrics.h"

static int run(int argc, char *argv[]) {
        _cleanup_(sd_json_variant_unrefp) sd_json_variant *ret = NULL;
        int r;

        /* This is mostly intended to be used for scripts which want
         * to detect whether we are being run in a virtualized
         * environment or not */

        log_setup();

        r = metrics_query_all(&ret);
        if (r < 0)
                return r;

        return sd_json_variant_dump(ret, SD_JSON_FORMAT_PRETTY_AUTO, stdout, NULL);
}

DEFINE_MAIN_FUNCTION_WITH_POSITIVE_FAILURE(run);
