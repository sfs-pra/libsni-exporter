# libsni-exporter

Small pure-C GLib/GIO library that exports a StatusNotifierItem (`org.kde.StatusNotifierItem`)
and a classic dbusmenu (`com.canonical.dbusmenu`) over D-Bus.

This code is extracted from the direct SNI implementation used in `sensindicator`,
without depending on `libayatana-appindicator`.

## What it provides

- direct SNI item export over session D-Bus
- direct dbusmenu export over session D-Bus
- simple C API wrapper around item + menu registration
- shared library and pkg-config metadata

## Current scope

Implemented:

- item title / status / tooltip updates
- ARGB pixmap icon updates (Cairo BGRA -> ARGB-BE conversion)
- icon by theme name
- icon theme path
- attention icon by ARGB pixmap or theme name
- activate / secondary activate / scroll callbacks
- simple flat action menus with separators
- disabled info items
- menu action callbacks
- menu label live update (host-dependent)
- full menu rebuild

Not implemented yet:

- submenus
- check/radio menu items
- overlay icon

## Build

```bash
meson setup build --prefix=/usr
meson compile -C build
```

## API style

The library exposes an opaque `SniExporter` handle with functions to:

- start and stop tray export on the session bus
- set title, status and tooltip
- set icon by ARGB pixmap or by theme name
- handle left click, middle click and scroll events
- build a simple flat dbusmenu
- receive menu activation callbacks

## Example

A C demo example is included in the package as:

`/usr/share/doc/libsni-exporter/minimal-tray.c`

and a matching `Makefile`:

`/usr/share/doc/libsni-exporter/Makefile`

Typical build command:

```bash
cc minimal-tray.c $(pkg-config --cflags --libs sni-exporter gio-2.0 glib-2.0) -o minimal-tray
```

Or simply:

```bash
make
```

What the demo shows:

- `sni_exporter_new_with_menu_path()`
- `sni_exporter_start()` / `sni_exporter_stop()`
- `sni_exporter_get_item_id()` / `get_menu_path()` / `get_bus_name()`
- dynamic title / status / tooltip updates
- dynamic ARGB icon updates for normal / warn / crit states
- flat menu creation with info items, actions and separators
- `sni_exporter_menu_update_label()` on live menu items
- full menu rebuild via `menu_begin()` / `menu_end()`
- action callback handling from the exported dbusmenu

## Comparison with libayatana-appindicator

| Feature | libayatana-appindicator | libsni-exporter |
|---|---|---|
| Icon by theme name | yes | **yes** |
| Icon by ARGB pixmap | yes | **yes** |
| Icon theme path | yes | **yes** |
| Title / Status / Tooltip | yes | **yes** |
| Flat menu with actions | yes (via GtkMenu) | **yes** (built-in) |
| Menu separators | yes | **yes** |
| Disabled info items | yes | **yes** |
| Menu action callbacks | yes | **yes** |
| Full menu rebuild | yes | **yes** |
| Menu label live update | yes | **yes** (host-dependent) |
| Activate callback (left click) | yes | **yes** |
| SecondaryActivate (middle click) | yes | **yes** |
| Scroll callback | yes | **yes** |
| Submenus | yes | not yet |
| Check/radio menu items | yes | not yet |
| Attention icon | yes | **yes** |
| Overlay icon | yes | not yet |
| XAyatanaLabel | yes | not yet |
| GtkMenu integration | yes | no (own menu API) |
| GObject type system | yes | no (opaque C struct) |
| X11 GtkStatusIcon fallback | yes | no (Wayland-focused) |
| Dependencies | gtk3 + dbusmenu-glib | GLib/GIO only |

## ARGB pixmap format

The SNI protocol expects icon pixels in **ARGB big-endian** byte order:
`byte[0]=A, byte[1]=R, byte[2]=G, byte[3]=B` per pixel.

Cairo (`CAIRO_FORMAT_ARGB32`) stores pixels in native byte order, which is
**BGRA** on little-endian (x86) systems. You must convert:

```c
argb_be[dst + 0] = cairo_raw[src + 3];  /* A */
argb_be[dst + 1] = cairo_raw[src + 2];  /* R */
argb_be[dst + 2] = cairo_raw[src + 1];  /* G */
argb_be[dst + 3] = cairo_raw[src + 0];  /* B */
```

See `minimal-tray.c` for a complete working example.

## ItemIsMenu behavior

The library defaults to `ItemIsMenu=true`. Call `sni_exporter_set_item_is_menu(tray, FALSE)`
before `sni_exporter_start()` to enable the `Activate` callback on left-click in hosts that
support it (Waybar, swaybar). Note: sfwbar always shows the menu regardless of this flag.

## Quick start

```c
#include "sni-exporter.h"

static void on_action(const char *id, void *data) {
    if (g_strcmp0(id, "quit") == 0)
        g_main_loop_quit((GMainLoop *)data);
}

int main(void) {
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    SniExporter *tray = sni_exporter_new("my-app");

    sni_exporter_set_title(tray, "My App");
    sni_exporter_set_icon_name(tray, "application-exit");
    sni_exporter_set_tooltip(tray, "My App", "Running");

    sni_exporter_menu_begin(tray);
    sni_exporter_menu_add_action_item(tray, "quit", "Quit", "application-exit");
    sni_exporter_menu_end(tray);

    sni_exporter_set_action_callback(tray, on_action, loop, NULL);
    sni_exporter_start(tray, NULL);

    g_main_loop_run(loop);
    sni_exporter_free(tray);
    g_main_loop_unref(loop);
    return 0;
}
```

Build: `cc app.c $(pkg-config --cflags --libs sni-exporter gio-2.0 glib-2.0) -o app`

## Intended use

This library is aimed at projects that need a small, direct SNI/dbusmenu exporter on
wlroots/KDE-compatible hosts without pulling in `libayatana-appindicator`.

Typical candidates: sensor monitors, battery indicators, simple status applets.

## Compatible SNI hosts

Tested or known-working hosts, in order of likely use:

| Host | Platform | Left-click | Notes |
|---|---|---|---|
| **sfwbar** | Wayland | Opens menu | Ignores `ItemsPropertiesUpdated` — use `menu_begin()`/`menu_end()` for updates |
| **Waybar** | Wayland | `Activate` or menu if `ItemIsMenu=true` | Needs `libdbusmenu-gtk3` |
| **swaybar** (Sway) | Wayland | Respects `ItemIsMenu` | Uses `sd-bus` directly |
| **Ironbar** | Wayland | Opens menu | Absolute file paths as icon names not resolved — use theme names |
| **Quickshell** | Wayland + X11 | `activate()` | QML/Qt-based |
| **nwg-panel** | Wayland | — | GTK3/Python |
| **KDE Plasma** | Wayland + X11 | Respects `ItemIsMenu` | Reference implementation |
| **LXQt panel** | X11 | `Activate` | Native since LXQt 0.10.0 |
| **XFCE4 panel** | X11 | `Activate` | Native since XFCE 4.15 (built into systray) |
| **GNOME Shell** | Wayland + X11 | — | Requires extension [#615](https://extensions.gnome.org/extension/615/) |
| **Cinnamon** | X11 | — | Native since 2.8 |
| **MATE panel** | X11 | — | Native since 1.18 |
| **i3 / openbox** | X11 | — | Via [snixembed](https://sr.ht/~steef/snixembed/) bridge |

### Watcher conflict

Only one `org.kde.StatusNotifierWatcher` can run at a time. If `kded6` (KDE daemon) starts before your bar, it steals the watcher and tray icons disappear. Fix: `pkill kded6`.
