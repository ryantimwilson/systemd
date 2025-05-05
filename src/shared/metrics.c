#include "dirent-util.h"
#include "fd-util.h"
#include "log.h"
#include "metrics.h"
#include "varlink-io.systemd.Metrics.h"
#include "varlink-util.h"

typedef struct MetricsIterator MetricsIterator;

DEFINE_PRIVATE_HASH_OPS_WITH_VALUE_DESTRUCTOR(link_hash_ops, void, trivial_hash_func, trivial_compare_func, sd_varlink, sd_varlink_unref);

struct MetricsIterator {
        Set *links;

        sd_json_variant *res;

        int error;

        sd_event *event;
};

static MetricsIterator* metrics_iterator_new(void) {
        MetricsIterator *i;

        i = new(MetricsIterator, 1);
        if (!i)
                return NULL;

        *i = (MetricsIterator) {};

        return i;
}

static MetricsIterator* metrics_iterator_free(MetricsIterator *iterator) {
        if (!iterator)
                return NULL;

        sd_json_variant_unref(iterator->res);
        set_free(iterator->links);
        sd_event_unref(iterator->event);

        return mfree(iterator);
}

DEFINE_TRIVIAL_CLEANUP_FUNC(MetricsIterator*, metrics_iterator_free);

static int metrics_on_query_reply(
        sd_varlink *link,
        sd_json_variant *parameters,
        const char *error_id,
        sd_varlink_reply_flags_t flags,
        void *userdata) {

        MetricsIterator *iterator = ASSERT_PTR(userdata);
        int r;

        if (error_id) {
                if (streq(error_id, SD_VARLINK_ERROR_TIMEOUT))
                        r = -ETIMEDOUT;
                else
                        r = -EIO;

                iterator->error = -r;
                goto finish;
        }

        r = sd_json_variant_merge_object(&iterator->res, parameters);
        if (r < 0)
                iterator->error = r;

finish:
        assert_se(set_remove(iterator->links, link) == link);
        link = sd_varlink_unref(link);
        return 0;
}

static int metrics_connect(MetricsIterator *iterator, const char *path) {
        _cleanup_(sd_varlink_unrefp) sd_varlink *vl = NULL;
        int r;

        assert(iterator);
        assert(path);

        r = sd_varlink_connect_address(&vl, path);
        if (r < 0)
                return log_debug_errno(r, "Unable to connect to %s: %m", path);

        sd_varlink_set_userdata(vl, iterator);

        if (!iterator->event) {
                r = sd_event_new(&iterator->event);
                if (r < 0)
                        return log_debug_errno(r, "Unable to allocate event loop: %m");
        }

        r = sd_varlink_attach_event(vl, iterator->event, SD_EVENT_PRIORITY_NORMAL);
        if (r < 0)
                return log_debug_errno(r, "Failed to attach varlink connection to event loop: %m");

        r = sd_varlink_bind_reply(vl, metrics_on_query_reply);
        if (r < 0)
                return log_debug_errno(r, "Failed to bind reply callback: %m");

        r = sd_varlink_invoke(vl, "io.systemd.Metrics.GetMetrics", NULL);
        if (r < 0)
                return log_debug_errno(r, "Failed to invoke varlink method: %m");

        r = set_ensure_consume(&iterator->links, &link_hash_ops, TAKE_PTR(vl));
        if (r < 0)
                return log_debug_errno(r, "Failed to add varlink connection to set: %m");
        return r;
}

static int metrics_start_query(MetricsIterator *iterator) {
        _cleanup_closedir_ DIR *d = NULL;
        int r;

        assert(iterator);

        d = opendir("/run/systemd/metrics/");
        if (!d) {
                if (errno == ENOENT)
                        return -ESRCH;

                return -errno;
        }

        FOREACH_DIRENT(de, d, return -errno) {
                _cleanup_free_ char *p = NULL;

                p = path_join("/run/systemd/metrics/", de->d_name);
                if (!p)
                        return -ENOMEM;

                r = metrics_connect(iterator, p);
                if (r < 0)
                        return r;
        }

        if (set_isempty(iterator->links))
                return -ESRCH; /* propagate last error we saw if we couldn't connect to anything. */

        /* We connected to some services, in this case, ignore the ones we failed on */
        return 0;
}

static int metrics_process(
        MetricsIterator *iterator,
        sd_json_variant **ret) {

        int r;

        assert(iterator);

        for (;;) {
                if (iterator->error < 0)
                        return iterator->error;

                if (set_isempty(iterator->links)) {
                        *ret = TAKE_PTR(iterator->res);
                        break;
                }

                r = sd_event_run(iterator->event, UINT64_MAX);
                if (r < 0)
                        return r;
        }

        return 0;
}

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

int metrics_query_all(sd_json_variant **ret) {
        _cleanup_(metrics_iterator_freep) MetricsIterator *iterator = NULL;
        int r;

        assert(ret);

        iterator = metrics_iterator_new();
        if (!iterator)
                return -ENOMEM;

        r = metrics_start_query(iterator);
        if (r < 0)
                return r;

        return metrics_process(iterator, ret);
}
