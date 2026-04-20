#include <glib.h>
#include <gio/gio.h>
#include "sni-exporter.h"

/* ── bus setup ─────────────────────────────────────── */

static GTestDBus *test_bus = NULL;

static void setup_bus(void) {
    test_bus = g_test_dbus_new(G_TEST_DBUS_NONE);
    g_test_dbus_up(test_bus);
}

static void teardown_bus(void) {
    g_test_dbus_down(test_bus);
    g_object_unref(test_bus);
    test_bus = NULL;
}

/* ── helpers ───────────────────────────────────────── */

static void drain(void) {
    /* Drain pending GLib main context events (D-Bus registration is async) */
    for (int i = 0; i < 10; i++) {
        g_main_context_iteration(NULL, FALSE);
    }
}

static SniExporter *make_tray(const char *item_id) {
    SniExporter *t = sni_exporter_new_with_menu_path(item_id, "/TestMenu");
    sni_exporter_start(t, NULL);
    drain();
    return t;
}

typedef struct {
    GVariant *result;
    GError *error;
    gboolean done;
} MenuCallState;

static void on_menu_call_done(GObject *source_object,
                              GAsyncResult *res,
                              gpointer user_data) {
    MenuCallState *state = user_data;
    state->result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source_object), res, &state->error);
    state->done = TRUE;
}

static GVariant *call_menu(SniExporter *tray, const char *method, GVariant *params) {
    GError *err = NULL;
    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
    g_assert_no_error(err);

    MenuCallState state = {0};
    g_dbus_connection_call(
        conn,
        sni_exporter_get_bus_name(tray),
        sni_exporter_get_menu_path(tray),
        "com.canonical.dbusmenu",
        method,
        params,
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        3000,
        NULL,
        on_menu_call_done,
        &state);

    while (!state.done) {
        g_main_context_iteration(NULL, TRUE);
    }

    g_assert_no_error(state.error);
    g_object_unref(conn);
    return state.result;
}

static GVariant *empty_group_properties_params(void) {
    GVariant *children[] = {
        g_variant_new_array(G_VARIANT_TYPE_INT32, NULL, 0),
        g_variant_new_strv(NULL, 0),
    };

    return g_variant_new_tuple(children, 2);
}

/* ── test 1: toggle item emits toggle-type (string) and toggle-state (int32) ── */

static void test_toggle_item_properties(void) {
    SniExporter *tray = make_tray("toggle-test");

    sni_exporter_menu_begin(tray);
    sni_exporter_menu_add_toggle_item(tray, "opt", "Option", NULL, TRUE);
    sni_exporter_menu_end(tray);
    drain();

    /* GetGroupProperties with empty ids list → returns all items */
    GVariant *reply = call_menu(tray, "GetGroupProperties",
                                 empty_group_properties_params());
    g_assert_nonnull(reply);

    GVariant *arr = g_variant_get_child_value(reply, 0);

    gboolean found_toggle = FALSE;
    for (gsize i = 0; i < g_variant_n_children(arr); i++) {
        GVariant *entry = g_variant_get_child_value(arr, i);
        gint32 id;
        GVariant *dict;
        g_variant_get(entry, "(i@a{sv})", &id, &dict);

        if (id == 0) { /* root node, skip */
            g_variant_unref(dict);
            g_variant_unref(entry);
            continue;
        }

        GVariant *tt = g_variant_lookup_value(dict, "toggle-type", G_VARIANT_TYPE_STRING);
        g_assert_nonnull(tt);
        g_assert_cmpstr(g_variant_get_string(tt, NULL), ==, "checkmark");
        g_variant_unref(tt);

        /* toggle-state MUST be int32, not boolean */
        GVariant *ts = g_variant_lookup_value(dict, "toggle-state", G_VARIANT_TYPE_INT32);
        g_assert_nonnull(ts);
        g_assert_cmpint(g_variant_get_int32(ts), ==, 1);
        g_variant_unref(ts);

        found_toggle = TRUE;
        g_variant_unref(dict);
        g_variant_unref(entry);
    }
    g_assert_true(found_toggle);

    g_variant_unref(arr);
    g_variant_unref(reply);
    sni_exporter_free(tray);
}

/* ── test 2: action item has NO toggle-type property ── */

static void test_action_item_no_toggle(void) {
    SniExporter *tray = make_tray("action-test");

    sni_exporter_menu_begin(tray);
    sni_exporter_menu_add_action_item(tray, "act", "Action", NULL);
    sni_exporter_menu_end(tray);
    drain();

    GVariant *reply = call_menu(tray, "GetGroupProperties",
                                 empty_group_properties_params());
    g_assert_nonnull(reply);

    GVariant *arr = g_variant_get_child_value(reply, 0);
    for (gsize i = 0; i < g_variant_n_children(arr); i++) {
        GVariant *entry = g_variant_get_child_value(arr, i);
        gint32 id;
        GVariant *dict;
        g_variant_get(entry, "(i@a{sv})", &id, &dict);
        if (id != 0) {
            GVariant *tt = g_variant_lookup_value(dict, "toggle-type", NULL);
            g_assert_null(tt);
        }
        g_variant_unref(dict);
        g_variant_unref(entry);
    }

    g_variant_unref(arr);
    g_variant_unref(reply);
    sni_exporter_free(tray);
}

/* ── test 3: event dispatch calls callback for action, not for separator ── */

static int g_callback_count = 0;

static void on_test_action(const char *id, void *data) {
    (void)id; (void)data;
    g_callback_count++;
}

static void test_event_dispatch(void) {
    SniExporter *tray = make_tray("event-test");
    g_callback_count = 0;

    sni_exporter_menu_begin(tray);
    sni_exporter_menu_add_action_item(tray, "act", "Action", NULL);
    sni_exporter_menu_add_separator(tray);
    sni_exporter_menu_end(tray);
    sni_exporter_set_action_callback(tray, on_test_action, NULL, NULL);
    drain();

    /* GetLayout to find the action item id (should be 1, root is 0) */
    /* Fire Event on item id=1 with type "clicked" */
    GVariant *reply = call_menu(tray, "Event",
        g_variant_new("(isvu)", 1, "clicked",
                      g_variant_new_int32(0), (guint32)0));
    drain();

    g_assert_cmpint(g_callback_count, ==, 1);

    if (reply) g_variant_unref(reply);
    sni_exporter_free(tray);
}

/* ── main ──────────────────────────────────────────── */

int main(int argc, char **argv) {
    setup_bus();
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/menu/toggle_item_properties", test_toggle_item_properties);
    g_test_add_func("/menu/action_item_no_toggle",  test_action_item_no_toggle);
    g_test_add_func("/menu/event_dispatch",         test_event_dispatch);

    int result = g_test_run();
    teardown_bus();
    return result;
}
