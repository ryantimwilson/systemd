#include "log.h"
#include "metrics.h"
#include "varlink-io.systemd.Metrics.h"
#include "varlink-util.h"

int metrics_setup_varlink_server(sd_varlink_server **m, sd_event *event, sd_varlink_method_t vl_method_get_metrics_cb, void *userdata, const char *socket) {
        _cleanup_(sd_varlink_server_unrefp) sd_varlink_server *s = NULL;
        int r;

        r = varlink_server_new(&s, SD_VARLINK_SERVER_INHERIT_USERDATA, userdata);
        if (r < 0)
                return log_error_errno(r, "Failed to allocate varlink server object: %m");

        r = sd_varlink_server_add_interface(s, &vl_interface_io_systemd_Metrics);
        if (r < 0)
                return log_error_errno(r, "Failed to add Metrics interface to varlink server: %m");

        r = sd_varlink_server_bind_method(
                        s, "io.systemd.Metrics.GetMetrics", vl_method_get_metrics_cb);
        if (r < 0)
                return r;

        r = sd_varlink_server_listen_address(s, socket, 0600);
        if (r < 0)
                return r;

        r = sd_varlink_server_attach_event(s, event, SD_EVENT_PRIORITY_NORMAL);
        if (r < 0)
                return r;

        *m = TAKE_PTR(s);
        return 0;
}
