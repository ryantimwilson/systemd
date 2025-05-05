#pragma once

#include "sd-varlink.h"

int metrics_fnmatch(const char *pattern, const char *name);

#define SD_JSON_BUILD_METRIC(p, m, ...) SD_JSON_BUILD_PAIR_CONDITION(metrics_fnmatch((p), (m)) == 0, (m), __VA_ARGS__)

int metrics_query_all(sd_json_variant **ret, const char *pattern);

int metrics_setup_varlink_server(sd_varlink_server **m, sd_event *event, sd_varlink_method_t vl_method_get_metrics_cb, void *userdata, const char *socket);
