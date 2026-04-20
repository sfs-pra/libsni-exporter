#include "sni-exporter.h"

#include <string.h>
#include <unistd.h>

#define SNI_OBJECT_PATH "/StatusNotifierItem"
#define SNI_IFACE "org.kde.StatusNotifierItem"
#define DBUSMENU_IFACE "com.canonical.dbusmenu"
#define WATCHER_BUS "org.kde.StatusNotifierWatcher"
#define WATCHER_PATH "/StatusNotifierWatcher"
#define WATCHER_IFACE "org.kde.StatusNotifierWatcher"
#define DEFAULT_MENU_PATH "/MenuBar"

typedef struct {
    gint id;
    char *action_id;
    char *label;
    gboolean enabled;
    gboolean visible;
    char *icon_name;
    gboolean separator;
    char *toggle_type;
    gboolean toggle_state;
} SniExporterMenuItem;

struct _SniExporter {
    char *item_id;
    char *menu_path;
    char *bus_name;

    char *title;
    char *status;
    char *tooltip_title;
    char *tooltip_body;

    guint8 *pixmap_data;
    gsize pixmap_len;
    gint pixmap_w;
    gint pixmap_h;
    gboolean has_pixmap;
    guint8 *attn_pixmap_data;
    gsize attn_pixmap_len;
    gint attn_pixmap_w;
    gint attn_pixmap_h;
    gboolean has_attn_pixmap;
    char *attn_icon_name;

    GDBusConnection *connection;
    guint bus_owner_id;
    guint item_reg_id;
    guint menu_reg_id;
    guint watcher_watch_id;
    gboolean watcher_ok;
    gboolean item_is_menu;

    GDBusNodeInfo *item_node_info;
    GDBusNodeInfo *menu_node_info;

    GPtrArray *menu_items;
    gint next_menu_id;
    gint next_info_index;
    guint menu_revision;

    SniExporterActionCallback action_callback;
    void *action_user_data;
    GDestroyNotify action_destroy_notify;

    char *icon_name;
    char *icon_theme_path;

    SniExporterActivateCallback activate_callback;
    void *activate_user_data;
    GDestroyNotify activate_destroy_notify;
    SniExporterActivateCallback secondary_activate_callback;
    void *secondary_activate_user_data;
    GDestroyNotify secondary_activate_destroy_notify;
    SniExporterScrollCallback scroll_callback;
    void *scroll_user_data;
    GDestroyNotify scroll_destroy_notify;
};

static const char *sni_item_xml =
    "<node>"
    "  <interface name='org.kde.StatusNotifierItem'>"
    "    <property name='Category' type='s' access='read'/>"
    "    <property name='Id' type='s' access='read'/>"
    "    <property name='Title' type='s' access='read'/>"
    "    <property name='Status' type='s' access='read'/>"
    "    <property name='WindowId' type='i' access='read'/>"
    "    <property name='IconThemePath' type='s' access='read'/>"
    "    <property name='Menu' type='o' access='read'/>"
    "    <property name='ItemIsMenu' type='b' access='read'/>"
    "    <property name='IconName' type='s' access='read'/>"
    "    <property name='IconPixmap' type='a(iiay)' access='read'/>"
    "    <property name='OverlayIconName' type='s' access='read'/>"
    "    <property name='OverlayIconPixmap' type='a(iiay)' access='read'/>"
    "    <property name='AttentionIconName' type='s' access='read'/>"
    "    <property name='AttentionIconPixmap' type='a(iiay)' access='read'/>"
    "    <property name='AttentionMovieName' type='s' access='read'/>"
    "    <property name='ToolTip' type='(sa(iiay)ss)' access='read'/>"
    "    <method name='ContextMenu'><arg name='x' type='i' direction='in'/><arg name='y' type='i' direction='in'/></method>"
    "    <method name='Activate'><arg name='x' type='i' direction='in'/><arg name='y' type='i' direction='in'/></method>"
    "    <method name='SecondaryActivate'><arg name='x' type='i' direction='in'/><arg name='y' type='i' direction='in'/></method>"
    "    <method name='Scroll'><arg name='delta' type='i' direction='in'/><arg name='orientation' type='s' direction='in'/></method>"
    "    <method name='ProvideXdgActivationToken'><arg name='token' type='s' direction='in'/></method>"
    "    <signal name='NewTitle'/>"
    "    <signal name='NewIcon'/>"
    "    <signal name='NewAttentionIcon'/>"
    "    <signal name='NewOverlayIcon'/>"
    "    <signal name='NewToolTip'/>"
    "    <signal name='NewStatus'><arg type='s' name='status'/></signal>"
    "    <signal name='NewMenu'/>"
    "  </interface>"
    "</node>";

static const char *dbusmenu_xml =
    "<node>"
    "  <interface name='com.canonical.dbusmenu'>"
    "    <method name='GetLayout'>"
    "      <arg name='parentId' type='i' direction='in'/>"
    "      <arg name='recursionDepth' type='i' direction='in'/>"
    "      <arg name='propertyNames' type='as' direction='in'/>"
    "      <arg name='revision' type='u' direction='out'/>"
    "      <arg name='layout' type='(ia{sv}av)' direction='out'/>"
    "    </method>"
    "    <method name='GetGroupProperties'>"
    "      <arg name='ids' type='ai' direction='in'/>"
    "      <arg name='propertyNames' type='as' direction='in'/>"
    "      <arg name='properties' type='a(ia{sv})' direction='out'/>"
    "    </method>"
    "    <method name='GetProperty'>"
    "      <arg name='id' type='i' direction='in'/>"
    "      <arg name='name' type='s' direction='in'/>"
    "      <arg name='value' type='v' direction='out'/>"
    "    </method>"
    "    <method name='Event'>"
    "      <arg name='id' type='i' direction='in'/>"
    "      <arg name='eventId' type='s' direction='in'/>"
    "      <arg name='data' type='v' direction='in'/>"
    "      <arg name='timestamp' type='u' direction='in'/>"
    "    </method>"
    "    <method name='EventGroup'>"
    "      <arg name='events' type='a(isvu)' direction='in'/>"
    "      <arg name='errors' type='ai' direction='out'/>"
    "    </method>"
    "    <method name='AboutToShow'><arg name='id' type='i' direction='in'/><arg name='needsUpdate' type='b' direction='out'/></method>"
    "    <method name='AboutToShowGroup'>"
    "      <arg name='ids' type='ai' direction='in'/>"
    "      <arg name='updatesNeeded' type='ai' direction='out'/>"
    "      <arg name='idErrors' type='ai' direction='out'/>"
    "    </method>"
    "    <signal name='ItemsPropertiesUpdated'>"
    "      <arg name='updatedProps' type='a(ia{sv})'/>"
    "      <arg name='removedProps' type='a(ias)'/>"
    "    </signal>"
    "    <signal name='LayoutUpdated'><arg name='revision' type='u'/><arg name='parent' type='i'/></signal>"
    "    <property name='Version' type='u' access='read'/>"
    "    <property name='TextDirection' type='s' access='read'/>"
    "    <property name='Status' type='s' access='read'/>"
    "    <property name='IconThemePath' type='as' access='read'/>"
    "  </interface>"
    "</node>";

static void menu_item_free(gpointer data) {
    SniExporterMenuItem *item = data;
    if (item == NULL) {
        return;
    }
    g_free(item->action_id);
    g_free(item->label);
    g_free(item->icon_name);
    g_free(item->toggle_type);
    g_free(item);
}

static char *sanitize_bus_name(const char *input) {
    GString *out = g_string_new(NULL);

    for (const char *p = input; *p != '\0'; p++) {
        char c = *p;
        gboolean ok = g_ascii_isalnum(c) || c == '.' || c == '_';
        g_string_append_c(out, ok ? c : '_');
    }

    return g_string_free(out, FALSE);
}

static char *build_bus_name(const char *item_id) {
    char *raw = g_strdup_printf("org.kde.StatusNotifierItem-%ld-%s", (long) getpid(), item_id);
    char *safe = sanitize_bus_name(raw);
    g_free(raw);
    return safe;
}

static GVariant *build_empty_pixmap(void) {
    return g_variant_parse(G_VARIANT_TYPE("a(iiay)"), "[]", NULL, NULL, NULL);
}

static GVariant *build_pixmap_variant(SniExporter *self) {
    GVariantBuilder outer_builder;

    if (!self->has_pixmap || self->pixmap_data == NULL || self->pixmap_len == 0) {
        return build_empty_pixmap();
    }

    GVariant **bv = g_new(GVariant *, self->pixmap_len);
    for (gsize i = 0; i < self->pixmap_len; i++) {
        bv[i] = g_variant_new_byte(self->pixmap_data[i]);
    }
    GVariant *bytes = g_variant_new_array(G_VARIANT_TYPE_BYTE, bv, self->pixmap_len);
    g_free(bv);

    g_variant_builder_init(&outer_builder, G_VARIANT_TYPE("a(iiay)"));
    g_variant_builder_add(&outer_builder, "(ii@ay)", self->pixmap_w, self->pixmap_h, bytes);
    return g_variant_builder_end(&outer_builder);
}

static GVariant *build_attention_pixmap_variant(SniExporter *self) {
    GVariantBuilder outer_builder;

    if (!self->has_attn_pixmap || self->attn_pixmap_data == NULL || self->attn_pixmap_len == 0)
        return build_empty_pixmap();

    GVariant **bv = g_new(GVariant *, self->attn_pixmap_len);
    for (gsize i = 0; i < self->attn_pixmap_len; i++)
        bv[i] = g_variant_new_byte(self->attn_pixmap_data[i]);
    GVariant *bytes = g_variant_new_array(G_VARIANT_TYPE_BYTE, bv, self->attn_pixmap_len);
    g_free(bv);

    g_variant_builder_init(&outer_builder, G_VARIANT_TYPE("a(iiay)"));
    g_variant_builder_add(&outer_builder, "(ii@ay)", self->attn_pixmap_w, self->attn_pixmap_h, bytes);
    return g_variant_builder_end(&outer_builder);
}

static GVariant *build_tooltip_variant(SniExporter *self) {
    return g_variant_new("(s@a(iiay)ss)", "", build_empty_pixmap(),
                         self->tooltip_title != NULL ? self->tooltip_title : "",
                         self->tooltip_body != NULL ? self->tooltip_body : "");
}

typedef struct {
    GDBusConnection *connection;
    char *name;
    GVariant *body;
} DeferredSignal;

static gboolean emit_item_signal_idle(gpointer user_data) {
    DeferredSignal *ds = user_data;

    g_dbus_connection_emit_signal(ds->connection, NULL, SNI_OBJECT_PATH, SNI_IFACE, ds->name, ds->body, NULL);

    g_free(ds->name);
    if (ds->body != NULL) {
        g_variant_unref(ds->body);
    }
    g_free(ds);
    return G_SOURCE_REMOVE;
}

static void emit_item_signal(SniExporter *self, const char *name, GVariant *body) {
    DeferredSignal *ds;

    if (self->connection == NULL || self->item_reg_id == 0) {
        if (body != NULL) {
            g_variant_unref(body);
        }
        return;
    }

    ds = g_new0(DeferredSignal, 1);
    ds->connection = self->connection;
    ds->name = g_strdup(name);
    if (body != NULL) {
        ds->body = g_variant_ref_sink(body);
    } else {
        ds->body = NULL;
    }

    g_idle_add(emit_item_signal_idle, ds);
}

static SniExporterMenuItem *menu_item_new(const char *action_id, const char *label, gboolean enabled,
                                          gboolean separator, const char *icon_name, gint id) {
    SniExporterMenuItem *item = g_new0(SniExporterMenuItem, 1);
    item->id = id;
    item->action_id = g_strdup(action_id != NULL ? action_id : "");
    item->label = g_strdup(label != NULL ? label : "");
    item->enabled = enabled;
    item->visible = TRUE;
    item->icon_name = g_strdup(icon_name != NULL ? icon_name : "");
    item->separator = separator;
    return item;
}

static SniExporterMenuItem *find_menu_item_by_id(SniExporter *self, gint id) {
    for (guint i = 0; i < self->menu_items->len; i++) {
        SniExporterMenuItem *item = g_ptr_array_index(self->menu_items, i);
        if (item->id == id) {
            return item;
        }
    }
    return NULL;
}

static SniExporterMenuItem *find_menu_item_by_action(SniExporter *self, const char *action_id) {
    for (guint i = 0; i < self->menu_items->len; i++) {
        SniExporterMenuItem *item = g_ptr_array_index(self->menu_items, i);
        if (g_strcmp0(item->action_id, action_id) == 0) {
            return item;
        }
    }
    return NULL;
}

static GVariant *build_menu_properties(const SniExporterMenuItem *item) {
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));

    if (item->separator) {
        g_variant_builder_add(&builder, "{sv}", "type", g_variant_new_string("separator"));
    } else {
        g_variant_builder_add(&builder, "{sv}", "label", g_variant_new_string(item->label));
        g_variant_builder_add(&builder, "{sv}", "enabled", g_variant_new_boolean(item->enabled));
        g_variant_builder_add(&builder, "{sv}", "visible", g_variant_new_boolean(item->visible));
        if (item->icon_name != NULL && item->icon_name[0] != '\0') {
            g_variant_builder_add(&builder, "{sv}", "icon-name", g_variant_new_string(item->icon_name));
        }
        if (item->toggle_type != NULL) {
            g_variant_builder_add(&builder, "{sv}", "toggle-type",
                                  g_variant_new_string(item->toggle_type));
            g_variant_builder_add(&builder, "{sv}", "toggle-state",
                                  g_variant_new_int32(item->toggle_state ? 1 : 0));
        }
        if (item->id == 0) {
            g_variant_builder_add(&builder, "{sv}", "children-display", g_variant_new_string("submenu"));
        }
    }

    return g_variant_builder_end(&builder);
}

static GVariant *build_menu_node(SniExporter *self, const SniExporterMenuItem *item, gint recursion_depth) {
    GVariantBuilder children;
    g_variant_builder_init(&children, G_VARIANT_TYPE("av"));

    if (item->id == 0 && recursion_depth != 0) {
        for (guint i = 0; i < self->menu_items->len; i++) {
            SniExporterMenuItem *child = g_ptr_array_index(self->menu_items, i);
            gint next_depth = recursion_depth > 0 ? recursion_depth - 1 : recursion_depth;
            g_variant_builder_add_value(&children, g_variant_new_variant(build_menu_node(self, child, next_depth)));
        }
    }

    return g_variant_new("(i@a{sv}av)", item->id, build_menu_properties(item), &children);
}

static SniExporterMenuItem *build_menu_root_item(void) {
    return menu_item_new("root", "", FALSE, FALSE, "", 0);
}

static void emit_layout_updated(SniExporter *self) {
    if (self->connection == NULL || self->menu_reg_id == 0) {
        return;
    }

    g_dbus_connection_emit_signal(self->connection, NULL, self->menu_path, DBUSMENU_IFACE,
                                  "LayoutUpdated", g_variant_new("(ui)", self->menu_revision, 0), NULL);
}

static void emit_item_property_updated(SniExporter *self, gint id, const char *label) {
    GVariantBuilder updated;
    GVariantBuilder props;
    GVariantBuilder removed;

    if (self->connection == NULL || self->menu_reg_id == 0) {
        return;
    }

    g_variant_builder_init(&updated, G_VARIANT_TYPE("a(ia{sv})"));
    g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_init(&removed, G_VARIANT_TYPE("a(ias)"));

    g_variant_builder_add(&props, "{sv}", "label", g_variant_new_string(label));
    g_variant_builder_add(&updated, "(i@a{sv})", id, g_variant_builder_end(&props));

    g_dbus_connection_emit_signal(self->connection, NULL, self->menu_path, DBUSMENU_IFACE,
                                  "ItemsPropertiesUpdated",
                                  g_variant_new("(@a(ia{sv})@a(ias))",
                                                g_variant_builder_end(&updated),
                                                g_variant_builder_end(&removed)),
                                  NULL);
}

static void dispatch_menu_event(SniExporter *self, gint id, const char *event_id) {
    SniExporterMenuItem *item;

    if (event_id != NULL && event_id[0] != '\0' && g_strcmp0(event_id, "clicked") != 0) {
        return;
    }

    item = find_menu_item_by_id(self, id);
    if (item == NULL || item->separator || !item->enabled) {
        return;
    }

    if (self->action_callback != NULL) {
        self->action_callback(item->action_id, self->action_user_data);
    }
}

static void try_register_with_watcher(SniExporter *self) {
    const char *unique_name;
    GError *error = NULL;

    if (self->connection == NULL || self->watcher_ok) {
        return;
    }

    unique_name = g_dbus_connection_get_unique_name(self->connection);
    if (unique_name == NULL || *unique_name == '\0') {
        return;
    }

    g_dbus_connection_call_sync(self->connection,
                                WATCHER_BUS,
                                WATCHER_PATH,
                                WATCHER_IFACE,
                                "RegisterStatusNotifierItem",
                                g_variant_new("(s)", unique_name),
                                NULL,
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                NULL,
                                &error);

    if (error != NULL) {
        self->watcher_ok = FALSE;
        g_error_free(error);
        return;
    }

    self->watcher_ok = TRUE;
}

static void on_watcher_appeared(GDBusConnection *connection, const gchar *name, const gchar *owner, gpointer user_data) {
    SniExporter *self = user_data;
    (void) connection;
    (void) name;
    (void) owner;
    self->watcher_ok = FALSE;
    try_register_with_watcher(self);
}

static void on_watcher_vanished(GDBusConnection *connection, const gchar *name, gpointer user_data) {
    SniExporter *self = user_data;
    (void) connection;
    (void) name;
    self->watcher_ok = FALSE;
}

static GVariant *item_get_property(GDBusConnection *connection, const gchar *sender, const gchar *object_path,
                                   const gchar *interface_name, const gchar *property_name, GError **error,
                                   gpointer user_data) {
    SniExporter *self = user_data;
    (void) connection;
    (void) sender;
    (void) object_path;
    (void) interface_name;
    (void) error;

    if (g_strcmp0(property_name, "Category") == 0) return g_variant_new_string("ApplicationStatus");
    if (g_strcmp0(property_name, "Id") == 0) return g_variant_new_string(self->item_id);
    if (g_strcmp0(property_name, "Title") == 0) return g_variant_new_string(self->title);
    if (g_strcmp0(property_name, "Status") == 0) return g_variant_new_string(self->status);
    if (g_strcmp0(property_name, "WindowId") == 0) return g_variant_new_int32(0);
    if (g_strcmp0(property_name, "IconThemePath") == 0) return g_variant_new_string(self->icon_theme_path != NULL ? self->icon_theme_path : "");
    if (g_strcmp0(property_name, "Menu") == 0) return g_variant_new_object_path(self->menu_path);
    if (g_strcmp0(property_name, "ItemIsMenu") == 0) return g_variant_new_boolean(self->item_is_menu);
    if (g_strcmp0(property_name, "IconName") == 0) return g_variant_new_string(self->icon_name != NULL ? self->icon_name : "");
    if (g_strcmp0(property_name, "IconPixmap") == 0) return build_pixmap_variant(self);
    if (g_strcmp0(property_name, "OverlayIconName") == 0) return g_variant_new_string("");
    if (g_strcmp0(property_name, "OverlayIconPixmap") == 0) return build_empty_pixmap();
    if (g_strcmp0(property_name, "AttentionIconName") == 0)
        return g_variant_new_string(self->attn_icon_name ? self->attn_icon_name : "");
    if (g_strcmp0(property_name, "AttentionIconPixmap") == 0)
        return build_attention_pixmap_variant(self);
    if (g_strcmp0(property_name, "AttentionMovieName") == 0) return g_variant_new_string("");
    if (g_strcmp0(property_name, "ToolTip") == 0) return build_tooltip_variant(self);
    return NULL;
}

static void item_method_call(GDBusConnection *connection, const gchar *sender, const gchar *object_path,
                             const gchar *interface_name, const gchar *method_name, GVariant *parameters,
                             GDBusMethodInvocation *invocation, gpointer user_data) {
    SniExporter *self = user_data;
    (void) connection;
    (void) sender;
    (void) object_path;
    (void) interface_name;

    if (g_strcmp0(method_name, "Activate") == 0) {
        gint x, y;
        g_variant_get(parameters, "(ii)", &x, &y);
        if (self->activate_callback != NULL) {
            self->activate_callback(x, y, self->activate_user_data);
        }
        g_dbus_method_invocation_return_value(invocation, NULL);
        return;
    }

    if (g_strcmp0(method_name, "SecondaryActivate") == 0) {
        gint x, y;
        g_variant_get(parameters, "(ii)", &x, &y);
        if (self->secondary_activate_callback != NULL) {
            self->secondary_activate_callback(x, y, self->secondary_activate_user_data);
        }
        g_dbus_method_invocation_return_value(invocation, NULL);
        return;
    }

    if (g_strcmp0(method_name, "Scroll") == 0) {
        gint delta;
        const char *orientation;
        g_variant_get(parameters, "(i&s)", &delta, &orientation);
        if (self->scroll_callback != NULL) {
            self->scroll_callback(delta, orientation, self->scroll_user_data);
        }
        g_dbus_method_invocation_return_value(invocation, NULL);
        return;
    }

    g_dbus_method_invocation_return_value(invocation, NULL);
}

static GVariant *menu_get_property(GDBusConnection *connection, const gchar *sender, const gchar *object_path,
                                   const gchar *interface_name, const gchar *property_name, GError **error,
                                   gpointer user_data) {
    (void) connection;
    (void) sender;
    (void) object_path;
    (void) interface_name;
    (void) error;
    (void) user_data;

    if (g_strcmp0(property_name, "Version") == 0) return g_variant_new_uint32(4);
    if (g_strcmp0(property_name, "TextDirection") == 0) return g_variant_new_string("ltr");
    if (g_strcmp0(property_name, "Status") == 0) return g_variant_new_string("normal");
    if (g_strcmp0(property_name, "IconThemePath") == 0) return g_variant_parse(G_VARIANT_TYPE("as"), "[]", NULL, NULL, NULL);
    return NULL;
}

static void menu_method_call(GDBusConnection *connection, const gchar *sender, const gchar *object_path,
                             const gchar *interface_name, const gchar *method_name, GVariant *parameters,
                             GDBusMethodInvocation *invocation, gpointer user_data) {
    SniExporter *self = user_data;
    (void) connection;
    (void) sender;
    (void) object_path;
    (void) interface_name;

    if (g_strcmp0(method_name, "GetLayout") == 0) {
        gint parent_id;
        gint recursion_depth;
        SniExporterMenuItem *root;
        SniExporterMenuItem *item;

        g_variant_get_child(parameters, 0, "i", &parent_id);
        g_variant_get_child(parameters, 1, "i", &recursion_depth);

        if (parent_id != 0) {
            item = find_menu_item_by_id(self, parent_id);
            if (item == NULL) {
                g_dbus_method_invocation_return_dbus_error(invocation,
                                                           "org.freedesktop.DBus.Error.InvalidArgs",
                                                           "Unknown parent id");
                return;
            }

            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(u@(ia{sv}av))",
                                                                self->menu_revision,
                                                                build_menu_node(self, item, recursion_depth)));
            return;
        }

        root = build_menu_root_item();
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(u@(ia{sv}av))",
                                                            self->menu_revision,
                                                            build_menu_node(self, root, recursion_depth)));
        menu_item_free(root);
        return;
    }

    if (g_strcmp0(method_name, "GetGroupProperties") == 0) {
        GVariant *ids_variant = g_variant_get_child_value(parameters, 0);
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ia{sv})"));

        if (g_variant_n_children(ids_variant) == 0) {
            for (guint i = 0; i < self->menu_items->len; i++) {
                SniExporterMenuItem *item = g_ptr_array_index(self->menu_items, i);
                g_variant_builder_add(&builder, "(i@a{sv})", item->id, build_menu_properties(item));
            }
        } else {
            for (gsize i = 0; i < g_variant_n_children(ids_variant); i++) {
                GVariant *child = g_variant_get_child_value(ids_variant, i);
                gint id = g_variant_get_int32(child);
                SniExporterMenuItem *item = find_menu_item_by_id(self, id);
                if (item != NULL) {
                    g_variant_builder_add(&builder, "(i@a{sv})", item->id, build_menu_properties(item));
                }
                g_variant_unref(child);
            }
        }

        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(@a(ia{sv}))", g_variant_builder_end(&builder)));
        g_variant_unref(ids_variant);
        return;
    }

    if (g_strcmp0(method_name, "GetProperty") == 0) {
        gint id;
        const char *name;
        SniExporterMenuItem *item;

        g_variant_get_child(parameters, 0, "i", &id);
        g_variant_get_child(parameters, 1, "&s", &name);
        item = find_menu_item_by_id(self, id);

        if (item == NULL) {
            g_dbus_method_invocation_return_dbus_error(invocation,
                                                       "org.freedesktop.DBus.Error.InvalidArgs",
                                                       "Unknown menu item id");
            return;
        }

        if (g_strcmp0(name, "type") == 0) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(v)", g_variant_new_string(item->separator ? "separator" : "")));
            return;
        }
        if (g_strcmp0(name, "label") == 0) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(v)", g_variant_new_string(item->label)));
            return;
        }
        if (g_strcmp0(name, "enabled") == 0) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(v)", g_variant_new_boolean(item->enabled)));
            return;
        }
        if (g_strcmp0(name, "visible") == 0) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(v)", g_variant_new_boolean(item->visible)));
            return;
        }
        if (g_strcmp0(name, "icon-name") == 0) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(v)", g_variant_new_string(item->icon_name)));
            return;
        }
        if (g_strcmp0(name, "children-display") == 0) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(v)", g_variant_new_string(item->id == 0 ? "submenu" : "")));
            return;
        }

        g_dbus_method_invocation_return_dbus_error(invocation,
                                                   "org.freedesktop.DBus.Error.InvalidArgs",
                                                   "Unknown property");
        return;
    }

        if (g_strcmp0(method_name, "Event") == 0) {
            gint id;
            const char *event_id;

            g_variant_get_child(parameters, 0, "i", &id);
            g_variant_get_child(parameters, 1, "&s", &event_id);
            dispatch_menu_event(self, id, event_id);
            g_dbus_method_invocation_return_value(invocation, NULL);
            return;
        }

    if (g_strcmp0(method_name, "EventGroup") == 0) {
        GVariant *events = g_variant_get_child_value(parameters, 0);
        for (gsize i = 0; i < g_variant_n_children(events); i++) {
            GVariant *ev = g_variant_get_child_value(events, i);
            GVariant *id_v = g_variant_get_child_value(ev, 0);
            GVariant *event_v = g_variant_get_child_value(ev, 1);
            gint id = g_variant_get_int32(id_v);
            const char *event_id = g_variant_get_string(event_v, NULL);
            dispatch_menu_event(self, id, event_id);
            g_variant_unref(id_v);
            g_variant_unref(event_v);
            g_variant_unref(ev);
        }
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(@ai)", g_variant_parse(G_VARIANT_TYPE("ai"), "[]", NULL, NULL, NULL)));
        g_variant_unref(events);
        return;
    }

    if (g_strcmp0(method_name, "AboutToShow") == 0) {
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", FALSE));
        return;
    }

    if (g_strcmp0(method_name, "AboutToShowGroup") == 0) {
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(@ai@ai)",
                                                            g_variant_parse(G_VARIANT_TYPE("ai"), "[]", NULL, NULL, NULL),
                                                            g_variant_parse(G_VARIANT_TYPE("ai"), "[]", NULL, NULL, NULL)));
        return;
    }

    g_dbus_method_invocation_return_dbus_error(invocation,
                                               "org.freedesktop.DBus.Error.UnknownMethod",
                                               "Unknown method");
}

static const GDBusInterfaceVTable item_vtable = {
    item_method_call,
    item_get_property,
    NULL,
};

static const GDBusInterfaceVTable menu_vtable = {
    menu_method_call,
    menu_get_property,
    NULL,
};

SniExporter *sni_exporter_new(const char *item_id) {
    return sni_exporter_new_with_menu_path(item_id, DEFAULT_MENU_PATH);
}

SniExporter *sni_exporter_new_with_menu_path(const char *item_id, const char *menu_object_path) {
    SniExporter *self;

    g_return_val_if_fail(item_id != NULL && *item_id != '\0', NULL);

    self = g_new0(SniExporter, 1);
    self->item_is_menu = TRUE;
    self->item_id = g_strdup(item_id);
    self->menu_path = g_strdup(menu_object_path != NULL ? menu_object_path : DEFAULT_MENU_PATH);
    self->title = g_strdup("");
    self->status = g_strdup("Active");
    self->tooltip_title = g_strdup("");
    self->tooltip_body = g_strdup("");
    self->icon_name = g_strdup("");
    self->icon_theme_path = g_strdup("");
    self->menu_items = g_ptr_array_new_with_free_func(menu_item_free);
    self->next_menu_id = 1;
    self->next_info_index = 0;
    self->menu_revision = 1;
    return self;
}

void sni_exporter_free(SniExporter *self) {
    if (self == NULL) {
        return;
    }

    sni_exporter_stop(self);

    if (self->action_destroy_notify != NULL && self->action_user_data != NULL) {
        self->action_destroy_notify(self->action_user_data);
    }

    if (self->activate_destroy_notify && self->activate_user_data) {
        self->activate_destroy_notify(self->activate_user_data);
    }
    if (self->secondary_activate_destroy_notify && self->secondary_activate_user_data) {
        self->secondary_activate_destroy_notify(self->secondary_activate_user_data);
    }
    if (self->scroll_destroy_notify && self->scroll_user_data) {
        self->scroll_destroy_notify(self->scroll_user_data);
    }

    g_free(self->item_id);
    g_free(self->menu_path);
    g_free(self->bus_name);
    g_free(self->title);
    g_free(self->status);
    g_free(self->tooltip_title);
    g_free(self->tooltip_body);
    g_free(self->icon_name);
    g_free(self->icon_theme_path);
    g_free(self->pixmap_data);
    g_free(self->attn_pixmap_data);
    g_free(self->attn_icon_name);
    if (self->menu_items != NULL) {
        g_ptr_array_unref(self->menu_items);
    }
    g_free(self);
}

gboolean sni_exporter_start(SniExporter *self, GError **error) {
    GDBusInterfaceInfo *item_iface;
    GDBusInterfaceInfo *menu_iface;

    g_return_val_if_fail(self != NULL, FALSE);

    if (self->connection != NULL) {
        return TRUE;
    }

    self->connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, error);
    if (self->connection == NULL) {
        return FALSE;
    }

    self->bus_name = build_bus_name(self->item_id);
    self->bus_owner_id = g_bus_own_name_on_connection(self->connection, self->bus_name,
                                                      G_BUS_NAME_OWNER_FLAGS_NONE, NULL, NULL, NULL, NULL);

    self->item_node_info = g_dbus_node_info_new_for_xml(sni_item_xml, error);
    if (self->item_node_info == NULL) {
        sni_exporter_stop(self);
        return FALSE;
    }

    self->menu_node_info = g_dbus_node_info_new_for_xml(dbusmenu_xml, error);
    if (self->menu_node_info == NULL) {
        sni_exporter_stop(self);
        return FALSE;
    }

    item_iface = g_dbus_node_info_lookup_interface(self->item_node_info, SNI_IFACE);
    menu_iface = g_dbus_node_info_lookup_interface(self->menu_node_info, DBUSMENU_IFACE);

    self->item_reg_id = g_dbus_connection_register_object(self->connection, SNI_OBJECT_PATH, item_iface,
                                                          &item_vtable, self, NULL, error);
    if (self->item_reg_id == 0) {
        sni_exporter_stop(self);
        return FALSE;
    }

    self->menu_reg_id = g_dbus_connection_register_object(self->connection, self->menu_path, menu_iface,
                                                          &menu_vtable, self, NULL, error);
    if (self->menu_reg_id == 0) {
        sni_exporter_stop(self);
        return FALSE;
    }

    self->watcher_watch_id = g_bus_watch_name_on_connection(self->connection,
                                                            WATCHER_BUS,
                                                            G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                            on_watcher_appeared,
                                                            on_watcher_vanished,
                                                            self,
                                                            NULL);

    try_register_with_watcher(self);
    return TRUE;
}

void sni_exporter_stop(SniExporter *self) {
    g_return_if_fail(self != NULL);

    if (self->watcher_watch_id != 0) {
        g_bus_unwatch_name(self->watcher_watch_id);
        self->watcher_watch_id = 0;
    }

    if (self->item_reg_id != 0 && self->connection != NULL) {
        g_dbus_connection_unregister_object(self->connection, self->item_reg_id);
        self->item_reg_id = 0;
    }

    if (self->menu_reg_id != 0 && self->connection != NULL) {
        g_dbus_connection_unregister_object(self->connection, self->menu_reg_id);
        self->menu_reg_id = 0;
    }

    if (self->bus_owner_id != 0) {
        g_bus_unown_name(self->bus_owner_id);
        self->bus_owner_id = 0;
    }

    if (self->item_node_info != NULL) {
        g_dbus_node_info_unref(self->item_node_info);
        self->item_node_info = NULL;
    }
    if (self->menu_node_info != NULL) {
        g_dbus_node_info_unref(self->menu_node_info);
        self->menu_node_info = NULL;
    }
    if (self->connection != NULL) {
        g_object_unref(self->connection);
        self->connection = NULL;
    }

    g_clear_pointer(&self->bus_name, g_free);
    self->watcher_ok = FALSE;
}

const char *sni_exporter_get_bus_name(SniExporter *self) {
    g_return_val_if_fail(self != NULL, NULL);
    return self->bus_name;
}

const char *sni_exporter_get_item_id(SniExporter *self) {
    g_return_val_if_fail(self != NULL, NULL);
    return self->item_id;
}

const char *sni_exporter_get_menu_path(SniExporter *self) {
    g_return_val_if_fail(self != NULL, NULL);
    return self->menu_path;
}

void sni_exporter_set_title(SniExporter *self, const char *title) {
    g_return_if_fail(self != NULL);
    g_free(self->title);
    self->title = g_strdup(title != NULL ? title : "");
    emit_item_signal(self, "NewTitle", NULL);
}

void sni_exporter_set_status(SniExporter *self, const char *status) {
    g_return_if_fail(self != NULL);
    g_free(self->status);
    self->status = g_strdup(status != NULL ? status : "Active");
    emit_item_signal(self, "NewStatus", g_variant_new("(s)", self->status));
}

void sni_exporter_set_tooltip(SniExporter *self, const char *title, const char *body) {
    g_return_if_fail(self != NULL);
    g_free(self->tooltip_title);
    g_free(self->tooltip_body);
    self->tooltip_title = g_strdup(title != NULL ? title : "");
    self->tooltip_body = g_strdup(body != NULL ? body : "");
    emit_item_signal(self, "NewToolTip", NULL);
}

void sni_exporter_set_icon_argb(SniExporter *self, int width, int height, const guint8 *argb, gsize length) {
    g_return_if_fail(self != NULL);

    g_free(self->pixmap_data);
    self->pixmap_data = NULL;
    self->pixmap_len = 0;
    self->pixmap_w = width;
    self->pixmap_h = height;
    self->has_pixmap = FALSE;

    if (argb != NULL && length > 0) {
        self->pixmap_data = g_memdup2(argb, length);
        self->pixmap_len = length;
        self->has_pixmap = TRUE;
    }

    emit_item_signal(self, "NewIcon", NULL);
}

void sni_exporter_set_icon_name(SniExporter *self, const char *icon_name) {
    g_return_if_fail(self != NULL);
    g_free(self->icon_name);
    self->icon_name = g_strdup(icon_name != NULL ? icon_name : "");
    emit_item_signal(self, "NewIcon", NULL);
}

void sni_exporter_set_attention_icon_argb(SniExporter *self,
                                          int width, int height,
                                          const guint8 *argb, gsize length) {
    g_return_if_fail(self != NULL && argb != NULL);
    g_return_if_fail(length == (gsize) width * (gsize) height * 4);

    g_free(self->attn_pixmap_data);
    self->attn_pixmap_data = NULL;
    self->attn_pixmap_len = 0;
    self->attn_pixmap_w = 0;
    self->attn_pixmap_h = 0;
    self->has_attn_pixmap = FALSE;

    if (width > 0 && height > 0) {
        self->attn_pixmap_data = g_memdup2(argb, length);
        self->attn_pixmap_len = length;
        self->attn_pixmap_w = width;
        self->attn_pixmap_h = height;
        self->has_attn_pixmap = TRUE;
    }
    emit_item_signal(self, "NewAttentionIcon", NULL);
}

void sni_exporter_set_attention_icon_name(SniExporter *self, const char *icon_name) {
    g_return_if_fail(self != NULL);
    g_free(self->attn_icon_name);
    self->attn_icon_name = g_strdup(icon_name);
    emit_item_signal(self, "NewAttentionIcon", NULL);
}

void sni_exporter_set_icon_theme_path(SniExporter *self, const char *path) {
    g_return_if_fail(self != NULL);
    g_free(self->icon_theme_path);
    self->icon_theme_path = g_strdup(path != NULL ? path : "");
    emit_item_signal(self, "NewIconThemePath", NULL);
}

void sni_exporter_set_item_is_menu(SniExporter *self, gboolean value) {
    g_return_if_fail(self != NULL);
    self->item_is_menu = value;
}

void sni_exporter_set_activate_callback(SniExporter *self,
                                        SniExporterActivateCallback callback,
                                        void *user_data,
                                        GDestroyNotify destroy_notify) {
    g_return_if_fail(self != NULL);

    if (self->activate_destroy_notify && self->activate_user_data) {
        self->activate_destroy_notify(self->activate_user_data);
    }

    self->activate_callback = callback;
    self->activate_user_data = user_data;
    self->activate_destroy_notify = destroy_notify;
}

void sni_exporter_set_secondary_activate_callback(SniExporter *self,
                                                  SniExporterActivateCallback callback,
                                                  void *user_data,
                                                  GDestroyNotify destroy_notify) {
    g_return_if_fail(self != NULL);

    if (self->secondary_activate_destroy_notify && self->secondary_activate_user_data) {
        self->secondary_activate_destroy_notify(self->secondary_activate_user_data);
    }

    self->secondary_activate_callback = callback;
    self->secondary_activate_user_data = user_data;
    self->secondary_activate_destroy_notify = destroy_notify;
}

void sni_exporter_set_scroll_callback(SniExporter *self,
                                      SniExporterScrollCallback callback,
                                      void *user_data,
                                      GDestroyNotify destroy_notify) {
    g_return_if_fail(self != NULL);

    if (self->scroll_destroy_notify && self->scroll_user_data) {
        self->scroll_destroy_notify(self->scroll_user_data);
    }

    self->scroll_callback = callback;
    self->scroll_user_data = user_data;
    self->scroll_destroy_notify = destroy_notify;
}

void sni_exporter_set_action_callback(SniExporter *self, SniExporterActionCallback callback,
                                      void *user_data, GDestroyNotify destroy_notify) {
    g_return_if_fail(self != NULL);

    if (self->action_destroy_notify != NULL && self->action_user_data != NULL) {
        self->action_destroy_notify(self->action_user_data);
    }

    self->action_callback = callback;
    self->action_user_data = user_data;
    self->action_destroy_notify = destroy_notify;
}

void sni_exporter_menu_begin(SniExporter *self) {
    g_return_if_fail(self != NULL);
    g_ptr_array_set_size(self->menu_items, 0);
    self->next_menu_id = 1;
    self->next_info_index = 0;
}

void sni_exporter_menu_add_info_item(SniExporter *self, const char *label) {
    char *action_id;
    SniExporterMenuItem *item;

    g_return_if_fail(self != NULL);

    if (self->next_info_index == 0) {
        action_id = g_strdup("sensor-info");
    } else if (self->next_info_index == 1) {
        action_id = g_strdup("value-info");
    } else {
        action_id = g_strdup_printf("info-%d", self->next_info_index + 1);
    }

    item = menu_item_new(action_id, label, FALSE, FALSE, "", self->next_menu_id++);
    self->next_info_index++;
    g_ptr_array_add(self->menu_items, item);
    g_free(action_id);
}

void sni_exporter_menu_add_separator(SniExporter *self) {
    char *action_id;

    g_return_if_fail(self != NULL);

    action_id = g_strdup_printf("separator-%d", self->next_menu_id);
    g_ptr_array_add(self->menu_items,
                    menu_item_new(action_id, "", FALSE, TRUE, "", self->next_menu_id++));
    g_free(action_id);
}

void sni_exporter_menu_add_action_item(SniExporter *self, const char *action_id, const char *label, const char *icon_name) {
    g_return_if_fail(self != NULL);
    g_return_if_fail(action_id != NULL);

    g_ptr_array_add(self->menu_items,
                    menu_item_new(action_id,
                                  label != NULL ? label : "",
                                  TRUE,
                                  FALSE,
                                  icon_name != NULL ? icon_name : "",
                                  self->next_menu_id++));
}

void sni_exporter_menu_add_toggle_item(SniExporter *self,
                                       const char *action_id,
                                       const char *label,
                                       const char *icon_name,
                                       gboolean checked) {
    SniExporterMenuItem *item;

    g_return_if_fail(self != NULL);
    g_return_if_fail(action_id != NULL);

    item = menu_item_new(action_id,
                         label != NULL ? label : "",
                         TRUE,
                         FALSE,
                         icon_name != NULL ? icon_name : "",
                         self->next_menu_id++);
    item->toggle_type = g_strdup("checkmark");
    item->toggle_state = checked;
    g_ptr_array_add(self->menu_items, item);
}

void sni_exporter_menu_end(SniExporter *self) {
    g_return_if_fail(self != NULL);
    self->menu_revision++;
    emit_layout_updated(self);
    /* Also emit the SNI-level NewMenu signal so hosts that cache the dbusmenu
     * layout (e.g. sfwbar) reconnect and call GetLayout afresh instead of
     * relying on a stale structural snapshot. */
    emit_item_signal(self, "NewMenu", NULL);
}

void sni_exporter_menu_update_label(SniExporter *self, const char *action_id, const char *new_label) {
    SniExporterMenuItem *item;

    g_return_if_fail(self != NULL);
    g_return_if_fail(action_id != NULL);

    item = find_menu_item_by_action(self, action_id);
    if (item == NULL) {
        return;
    }
    if (g_strcmp0(item->label, new_label) == 0) {
        return;
    }

    g_free(item->label);
    item->label = g_strdup(new_label != NULL ? new_label : "");
    emit_item_property_updated(self, item->id, item->label);
}

void sni_exporter_menu_set_toggle_state(SniExporter *self,
                                        const char *action_id,
                                        gboolean checked) {
    SniExporterMenuItem *item;
    GVariantBuilder updated;
    GVariantBuilder removed;

    g_return_if_fail(self != NULL);
    g_return_if_fail(action_id != NULL);

    item = find_menu_item_by_action(self, action_id);
    if (item == NULL) {
        return;
    }

    item->toggle_state = checked;

    if (self->connection == NULL || self->menu_reg_id == 0) {
        return;
    }

    g_variant_builder_init(&updated, G_VARIANT_TYPE("a(ia{sv})"));
    g_variant_builder_add(&updated, "(i@a{sv})", item->id, build_menu_properties(item));

    g_variant_builder_init(&removed, G_VARIANT_TYPE("a(ias)"));

    g_dbus_connection_emit_signal(self->connection, NULL, self->menu_path, DBUSMENU_IFACE,
                                  "ItemsPropertiesUpdated",
                                  g_variant_new("(@a(ia{sv})@a(ias))",
                                                g_variant_builder_end(&updated),
                                                g_variant_builder_end(&removed)),
                                  NULL);
}
