#pragma once

#include "sd-varlink.h"

int metrics_query_all(sd_json_variant **ret);

int metrics_setup_varlink_server(sd_varlink_server **m, sd_event *event, sd_varlink_method_t vl_method_get_metrics_cb, void *userdata, const char *socket);
