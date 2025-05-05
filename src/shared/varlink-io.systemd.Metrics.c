/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "varlink-io.systemd.Metrics.h"

static SD_VARLINK_DEFINE_METHOD(GetMetrics);

SD_VARLINK_DEFINE_INTERFACE(
                io_systemd_Metrics,
                "io.systemd.Metrics",
                SD_VARLINK_INTERFACE_COMMENT("Metrics APIs"),
                SD_VARLINK_SYMBOL_COMMENT(
                                "Return key -> value dictionary of relevant metrics, optionally filtered by object."),
                &vl_method_GetMetrics);
