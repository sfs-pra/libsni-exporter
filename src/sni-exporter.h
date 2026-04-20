#ifndef SNI_EXPORTER_H
#define SNI_EXPORTER_H

#include <gio/gio.h>

G_BEGIN_DECLS

/**
 * Opaque handle for an SNI tray exporter.
 * Manages a StatusNotifierItem and a com.canonical.dbusmenu on the session bus.
 */
typedef struct _SniExporter SniExporter;

/**
 * Callback invoked when a menu action item is clicked.
 * @param action_id  The action identifier passed to sni_exporter_menu_add_action_item().
 * @param user_data  User data registered with sni_exporter_set_action_callback().
 */
typedef void (*SniExporterActionCallback)(const char *action_id, void *user_data);

/**
 * Callback invoked on Activate (left click) or SecondaryActivate (middle click).
 * @param x  X coordinate of the click (from host, may be 0).
 * @param y  Y coordinate of the click (from host, may be 0).
 * @param user_data  User data registered with the corresponding setter.
 *
 * Note: most SNI hosts call Activate only when ItemIsMenu is false.
 * ItemIsMenu defaults to true; call sni_exporter_set_item_is_menu(self, FALSE)
 * if you want hosts to use Activate on left click.
 */
typedef void (*SniExporterActivateCallback)(int x, int y, void *user_data);

/**
 * Callback invoked on mouse wheel scroll over the tray icon.
 * @param delta        Scroll amount (positive = up/right, negative = down/left).
 * @param orientation  "horizontal" or "vertical".
 * @param user_data    User data registered with sni_exporter_set_scroll_callback().
 */
typedef void (*SniExporterScrollCallback)(int delta, const char *orientation, void *user_data);

/* ── Lifecycle ── */

/** Create an exporter with default menu path "/MenuBar". */
SniExporter *sni_exporter_new(const char *item_id);

/** Create an exporter with a custom D-Bus menu object path. */
SniExporter *sni_exporter_new_with_menu_path(const char *item_id, const char *menu_object_path);

/** Free the exporter. Calls sni_exporter_stop() if still running. */
void sni_exporter_free(SniExporter *self);

/** Register on the session bus and connect to the SNI watcher. Returns FALSE on error. */
gboolean sni_exporter_start(SniExporter *self, GError **error);

/** Unregister from D-Bus and release all resources. Safe to call multiple times. */
void sni_exporter_stop(SniExporter *self);

/* ── Introspection ── */

/** Get the owned D-Bus bus name (available after start). */
const char *sni_exporter_get_bus_name(SniExporter *self);

/** Get the item id passed to the constructor. */
const char *sni_exporter_get_item_id(SniExporter *self);

/** Get the D-Bus object path of the exported menu. */
const char *sni_exporter_get_menu_path(SniExporter *self);

/* ── Item properties ── */

/** Set the tray icon title. Emits NewTitle signal. */
void sni_exporter_set_title(SniExporter *self, const char *title);

/**
 * Set the SNI status string.
 * Common values: "Active", "Passive", "NeedsAttention".
 * Note: when status is "NeedsAttention", hosts use AttentionIconPixmap instead of IconPixmap.
 * Set an attention icon with sni_exporter_set_attention_icon_argb() or
 * sni_exporter_set_attention_icon_name() before switching to "NeedsAttention".
 * Emits NewStatus signal.
 */
void sni_exporter_set_status(SniExporter *self, const char *status);

/** Set the tooltip title and body. Emits NewToolTip signal. */
void sni_exporter_set_tooltip(SniExporter *self, const char *title, const char *body);

/**
 * Set the tray icon from an ARGB pixmap buffer.
 *
 * The buffer must contain pixels in ARGB big-endian byte order:
 *   byte[0]=A, byte[1]=R, byte[2]=G, byte[3]=B for each pixel.
 *
 * If rendering with Cairo (CAIRO_FORMAT_ARGB32), Cairo stores pixels
 * in native byte order as BGRA on little-endian systems. Convert with:
 *
 *   argb_be[dst+0] = cairo_data[src+3];  // A
 *   argb_be[dst+1] = cairo_data[src+2];  // R
 *   argb_be[dst+2] = cairo_data[src+1];  // G
 *   argb_be[dst+3] = cairo_data[src+0];  // B
 *
 * @param width   Pixmap width in pixels.
 * @param height  Pixmap height in pixels.
 * @param argb    Pixel data in ARGB-BE byte order.
 * @param length  Total byte length of argb (must be width * height * 4).
 *
 * Emits NewIcon signal.
 */
void sni_exporter_set_icon_argb(SniExporter *self, int width, int height, const guint8 *argb, gsize length);

/**
 * Set the tray icon by theme icon name (e.g. "battery-full").
 * The host will look up the icon in its icon theme.
 * Emits NewIcon signal.
 */
void sni_exporter_set_icon_name(SniExporter *self, const char *icon_name);

/**
 * Set a custom icon theme search path.
 * Useful when icons are installed outside standard theme directories.
 * Emits NewIconThemePath signal.
 */
void sni_exporter_set_icon_theme_path(SniExporter *self, const char *path);

/**
 * Control whether left-click shows the dbusmenu (TRUE) or calls Activate (FALSE).
 * Default: TRUE. Most SNI hosts show the menu on left-click when TRUE.
 * Set FALSE to enable the Activate callback on left-click in hosts that support it
 * (Waybar, swaybar). Note: sfwbar always shows the menu regardless of this flag.
 * Call before sni_exporter_start().
 */
void sni_exporter_set_item_is_menu(SniExporter *self, gboolean value);

/**
 * Set the attention icon from an ARGB pixmap buffer.
 * Same byte-order requirements as sni_exporter_set_icon_argb() (ARGB big-endian).
 * @param width   Pixmap width in pixels.
 * @param height  Pixmap height in pixels.
 * @param argb    Pixel data in ARGB-BE byte order.
 * @param length  Total byte length of argb (must be width * height * 4).
 * Used when status is "NeedsAttention". Emits NewAttentionIcon signal.
 * Set before calling sni_exporter_set_status(self, "NeedsAttention").
 */
void sni_exporter_set_attention_icon_argb(SniExporter *self,
                                          int width, int height,
                                          const guint8 *argb, gsize length);

/**
 * Set the attention icon by theme icon name.
 * Used when status is "NeedsAttention". Emits NewAttentionIcon signal.
 */
void sni_exporter_set_attention_icon_name(SniExporter *self, const char *icon_name);

/* ── Event callbacks ── */

/**
 * Set the callback for menu action item clicks.
 * @param callback        Function called with the action_id of the clicked item.
 * @param user_data       Passed to callback.
 * @param destroy_notify  Called with user_data when callback is replaced or exporter freed. May be NULL.
 */
void sni_exporter_set_action_callback(
    SniExporter *self,
    SniExporterActionCallback callback,
    void *user_data,
    GDestroyNotify destroy_notify
);

/**
 * Set the Activate callback (left click on tray icon).
 * Note: only called by the host when ItemIsMenu is false.
 * @param callback     Function called on left-click activation.
 * @param user_data    Passed to callback. Ownership follows destroy_notify.
 * @param destroy_notify  Called with user_data when callback is replaced or exporter freed. May be NULL.
 */
void sni_exporter_set_activate_callback(
    SniExporter *self,
    SniExporterActivateCallback callback,
    void *user_data,
    GDestroyNotify destroy_notify
);

/**
 * Set the SecondaryActivate callback (middle click on tray icon).
 * @param callback     Function called on middle-click activation.
 * @param user_data    Passed to callback. Ownership follows destroy_notify.
 * @param destroy_notify  Called with user_data when callback is replaced or exporter freed. May be NULL.
 */
void sni_exporter_set_secondary_activate_callback(
    SniExporter *self,
    SniExporterActivateCallback callback,
    void *user_data,
    GDestroyNotify destroy_notify
);

/**
 * Set the Scroll callback (mouse wheel on tray icon).
 * @param callback     Function called on scroll.
 * @param user_data    Passed to callback. Ownership follows destroy_notify.
 * @param destroy_notify  Called with user_data when callback is replaced or exporter freed. May be NULL.
 */
void sni_exporter_set_scroll_callback(
    SniExporter *self,
    SniExporterScrollCallback callback,
    void *user_data,
    GDestroyNotify destroy_notify
);

/* ── Menu building ── */

/** Begin a new menu. Clears all existing items. Must be paired with sni_exporter_menu_end(). */
void sni_exporter_menu_begin(SniExporter *self);

/** Add a disabled info label (not clickable). */
void sni_exporter_menu_add_info_item(SniExporter *self, const char *label);

/** Add a visual separator line. */
void sni_exporter_menu_add_separator(SniExporter *self);

/**
 * Add a clickable action item.
 * @param action_id  Identifier passed to the action callback on click.
 * @param label      Display text.
 * @param icon_name  Theme icon name for the item (may be "" or NULL for no icon).
 */
void sni_exporter_menu_add_action_item(SniExporter *self, const char *action_id, const char *label, const char *icon_name);

/**
 * Add a checkbox (toggle) menu item.
 * The action callback fires with action_id when clicked, regardless of state.
 * The host toggles the visual state on click; call sni_exporter_menu_set_toggle_state()
 * to keep the stored state in sync with the host.
 * @param action_id  Identifier passed to the action callback on click.
 * @param label      Display text.
 * @param icon_name  Theme icon name (may be "" or NULL).
 * @param checked    Initial checked state (TRUE = checked).
 */
void sni_exporter_menu_add_toggle_item(SniExporter *self,
                                       const char *action_id,
                                       const char *label,
                                       const char *icon_name,
                                       gboolean checked);

/** Finish menu construction and emit LayoutUpdated signal. */
void sni_exporter_menu_end(SniExporter *self);

/**
 * Update the label of an existing menu item without full rebuild.
 * Emits ItemsPropertiesUpdated signal.
 * Note: some hosts (e.g. sfwbar) may not visually update from this signal.
 * Use sni_exporter_menu_begin() / menu_end() for guaranteed updates.
 */
void sni_exporter_menu_update_label(SniExporter *self, const char *action_id, const char *new_label);

/**
 * Update the checked state of an existing toggle item without full menu rebuild.
 * Emits ItemsPropertiesUpdated. Note: some hosts (e.g. sfwbar) may not update
 * visually from this signal — use menu_begin()/menu_end() for guaranteed updates.
 */
void sni_exporter_menu_set_toggle_state(SniExporter *self,
                                        const char *action_id,
                                        gboolean checked);

G_END_DECLS

#endif
