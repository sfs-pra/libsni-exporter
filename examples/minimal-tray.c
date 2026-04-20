/*
 * minimal-tray.c — libsni-exporter feature demo
 *
 * Demonstrates the public API of libsni-exporter:
 *
 *   - sni_exporter_new_with_menu_path() — create exporter with custom menu path
 *   - sni_exporter_start() / sni_exporter_stop() — lifecycle management
 *   - sni_exporter_get_item_id() / get_menu_path() / get_bus_name() — introspection
 *   - sni_exporter_set_title() — dynamic title updates
 *   - sni_exporter_set_status() — SNI status string
 *   - sni_exporter_set_tooltip() — tooltip with title and body
 *   - sni_exporter_set_icon_argb() — ARGB pixmap icon (Cairo rendering + BGRA->ARGB-BE)
 *   - sni_exporter_set_action_callback() — menu action handler
 *   - sni_exporter_set_activate_callback() — left click on tray icon
 *   - sni_exporter_set_secondary_activate_callback() — middle click on tray icon
 *   - sni_exporter_set_scroll_callback() — mouse wheel scroll on tray icon
 *   - sni_exporter_menu_begin() / menu_end() — full menu rebuild
 *   - sni_exporter_menu_add_info_item() — disabled info label
 *   - sni_exporter_menu_add_action_item() — clickable actions with icons
 *   - sni_exporter_menu_add_toggle_item() — checkbox menu item
 *   - sni_exporter_menu_add_separator() — visual separators
 *   - sni_exporter_menu_set_toggle_state() — update toggle state without rebuild
 *
 * Runtime behavior:
 *   - Tray icon appears as a colored 16x16 square
 *   - Every second: tick counter increments, tooltip updates
 *   - Every 5 seconds: state cycles normal -> warn -> crit (icon color changes)
 *   - Left click on icon: cycles state immediately
 *   - Middle click / scroll: prints event info to stdout
 *   - Menu actions: set state manually, switch menu layout, print D-Bus info, quit
 *   - "Verbose" toggle in menu: demonstrates checkbox items and set_toggle_state()
 *
 * Build:  make
 * Run:    ./minimal-tray
 *
 * ---------------------------------------------------------------------------
 *
 * minimal-tray.c — демонстрация API libsni-exporter
 *
 * Показывает публичный API библиотеки libsni-exporter:
 *
 *   - sni_exporter_new_with_menu_path() — создание экспортера с произвольным menu path
 *   - sni_exporter_start() / sni_exporter_stop() — управление жизненным циклом
 *   - sni_exporter_get_item_id() / get_menu_path() / get_bus_name() — информация о подключении
 *   - sni_exporter_set_title() — динамическое обновление заголовка
 *   - sni_exporter_set_status() — строка статуса SNI
 *   - sni_exporter_set_tooltip() — tooltip с заголовком и телом
 *   - sni_exporter_set_icon_argb() — ARGB pixmap-иконка (Cairo рендер + BGRA->ARGB-BE)
 *   - sni_exporter_set_action_callback() — обработчик действий меню
 *   - sni_exporter_set_activate_callback() — левый клик по иконке в трее
 *   - sni_exporter_set_secondary_activate_callback() — средний клик по иконке
 *   - sni_exporter_set_scroll_callback() — прокрутка колеса мыши на иконке
 *   - sni_exporter_menu_begin() / menu_end() — полная перестройка меню
 *   - sni_exporter_menu_add_info_item() — неактивная информационная строка
 *   - sni_exporter_menu_add_action_item() — кликабельные действия с иконками
 *   - sni_exporter_menu_add_toggle_item() — пункт меню с флажком (checkbox)
 *   - sni_exporter_menu_add_separator() — визуальные разделители
 *   - sni_exporter_menu_set_toggle_state() — обновление состояния флажка без полной перестройки
 *
 * Поведение при запуске:
 *   - В трее появляется цветной квадрат 16x16
 *   - Каждую секунду: счётчик тиков увеличивается, tooltip обновляется
 *   - Каждые 5 секунд: состояние переключается normal -> warn -> crit (цвет иконки меняется)
 *   - Левый клик по иконке: немедленно переключает состояние
 *   - Средний клик / прокрутка: выводит информацию о событии в stdout
 *   - Действия меню: ручная смена состояния, переключение layout меню, вывод D-Bus-информации, выход
 *   - Пункт "Verbose": демонстрация checkbox-элемента и set_toggle_state()
 *
 * Сборка:  make
 * Запуск:  ./minimal-tray
 */
#include <glib.h>
#include <cairo/cairo.h>

#include "sni-exporter.h"

typedef enum {
    DEMO_STATE_NORMAL,
    DEMO_STATE_WARN,
    DEMO_STATE_CRIT,
} DemoState;

typedef struct {
    GMainLoop *loop;
    SniExporter *tray;
    guint tick;
    gboolean alternate_menu;
    DemoState state;
    gboolean verbose;
} ExampleApp;

static const char *state_name(DemoState state) {
    switch (state) {
    case DEMO_STATE_NORMAL:
        return "normal";
    case DEMO_STATE_WARN:
        return "warn";
    case DEMO_STATE_CRIT:
        return "crit";
    default:
        return "unknown";
    }
}

static const char *state_sni_status(DemoState state) {
    (void) state;
    return "Active";
}

static void print_exporter_info(ExampleApp *app) {
    g_print("item-id:  %s\n", sni_exporter_get_item_id(app->tray));
    g_print("menu-path: %s\n", sni_exporter_get_menu_path(app->tray));
    g_print("bus-name:  %s\n", sni_exporter_get_bus_name(app->tray));
}

static void fill_demo_icon(guint8 *argb_be, int width, int height, DemoState state) {
    double fill_r = 0.333;
    double fill_g = 0.722;
    double fill_b = 0.420;

    if (state == DEMO_STATE_WARN) {
        fill_r = 0.831;
        fill_g = 0.627;
        fill_b = 0.090;
    } else if (state == DEMO_STATE_CRIT) {
        fill_r = 1.0;
        fill_g = 0.0;
        fill_b = 0.0;
    }

    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(surface);

    cairo_set_source_rgba(cr, fill_r, fill_g, fill_b, 1.0);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 0.125, 0.125, 0.125, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, 0.5, 0.5, width - 1, height - 1);
    cairo_stroke(cr);

    cairo_destroy(cr);
    cairo_surface_flush(surface);

    unsigned char *raw = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);

    for (int y = 0; y < height; y++) {
        int row_offset = y * stride;
        for (int x = 0; x < width; x++) {
            int src = row_offset + x * 4;
            int dst = (y * width + x) * 4;
            guint8 b_val = raw[src + 0];
            guint8 g_val = raw[src + 1];
            guint8 r_val = raw[src + 2];
            guint8 a_val = raw[src + 3];
            argb_be[dst + 0] = a_val;
            argb_be[dst + 1] = r_val;
            argb_be[dst + 2] = g_val;
            argb_be[dst + 3] = b_val;
        }
    }

    cairo_surface_destroy(surface);
}

static void update_live_info(ExampleApp *app) {
    char *state_upper = g_ascii_strup(state_name(app->state), -1);
    char *title = g_strdup_printf("libsni-exporter demo [%s]", state_upper);
    char *tooltip = g_strdup_printf("Tick: %u", app->tick);
    guint8 icon[16 * 16 * 4];

    fill_demo_icon(icon, 16, 16, app->state);

    sni_exporter_set_title(app->tray, title);
    sni_exporter_set_status(app->tray, state_sni_status(app->state));
    sni_exporter_set_tooltip(app->tray, title, tooltip);
    sni_exporter_set_icon_argb(app->tray, 16, 16, icon, sizeof(icon));

    g_free(state_upper);
    g_free(title);
    g_free(tooltip);
}

static void rebuild_menu(ExampleApp *app) {
    sni_exporter_menu_begin(app->tray);
    sni_exporter_menu_add_info_item(app->tray, "libsni-exporter demo");
    sni_exporter_menu_add_separator(app->tray);

    if (app->alternate_menu) {
        sni_exporter_menu_add_action_item(app->tray, "print-info", "Print exporter info", "dialog-information");
        sni_exporter_menu_add_action_item(app->tray, "set-crit", "Set critical state", "dialog-error");
        sni_exporter_menu_add_action_item(app->tray, "set-warn", "Set warning state", "dialog-warning");
        sni_exporter_menu_add_action_item(app->tray, "set-normal", "Set normal state", "emblem-default");
    } else {
        sni_exporter_menu_add_action_item(app->tray, "set-normal", "Set normal state", "emblem-default");
        sni_exporter_menu_add_action_item(app->tray, "set-warn", "Set warning state", "dialog-warning");
        sni_exporter_menu_add_action_item(app->tray, "set-crit", "Set critical state", "dialog-error");
        sni_exporter_menu_add_action_item(app->tray, "print-info", "Print exporter info", "dialog-information");
    }

    sni_exporter_menu_add_action_item(app->tray, "rebuild-menu", "Switch menu layout", "view-refresh");
    sni_exporter_menu_add_separator(app->tray);
    sni_exporter_menu_add_toggle_item(app->tray, "toggle-verbose",
                                      "Verbose logging", NULL, app->verbose);
    sni_exporter_menu_add_separator(app->tray);
    sni_exporter_menu_add_action_item(app->tray, "quit", "Quit", "application-exit");
    sni_exporter_menu_end(app->tray);

    update_live_info(app);
}

static void set_state(ExampleApp *app, DemoState state) {
    app->state = state;
    update_live_info(app);
}

static void cycle_state(ExampleApp *app) {
    switch (app->state) {
    case DEMO_STATE_NORMAL:
        set_state(app, DEMO_STATE_WARN);
        break;
    case DEMO_STATE_WARN:
        set_state(app, DEMO_STATE_CRIT);
        break;
    case DEMO_STATE_CRIT:
    default:
        set_state(app, DEMO_STATE_NORMAL);
        break;
    }
}

static gboolean on_tick(gpointer user_data) {
    ExampleApp *app = user_data;

    app->tick++;
    if (app->tick % 5 == 0) {
        cycle_state(app);
    } else {
        update_live_info(app);
    }

    return G_SOURCE_CONTINUE;
}

static void on_activate(int x, int y, void *user_data) {
    ExampleApp *app = user_data;
    g_print("Activate at (%d, %d)\n", x, y);
    if (app->verbose) {
        g_print("  [verbose] state before: %s\n", state_name(app->state));
    }
    cycle_state(app);
    if (app->verbose) {
        g_print("  [verbose] state after: %s\n", state_name(app->state));
    }
}

static void on_secondary_activate(int x, int y, void *user_data) {
    (void) user_data;
    g_print("SecondaryActivate at (%d, %d)\n", x, y);
}

static void on_scroll(int delta, const char *orientation, void *user_data) {
    ExampleApp *app = user_data;
    g_print("Scroll delta=%d orientation=%s\n", delta, orientation);
    if (app->verbose) {
        g_print("  [verbose] tick=%u state=%s\n", app->tick, state_name(app->state));
    }
}

static void on_action(const char *action_id, void *user_data) {
    ExampleApp *app = user_data;

    if (g_strcmp0(action_id, "set-normal") == 0) {
        set_state(app, DEMO_STATE_NORMAL);
        return;
    }
    if (g_strcmp0(action_id, "set-warn") == 0) {
        set_state(app, DEMO_STATE_WARN);
        return;
    }
    if (g_strcmp0(action_id, "set-crit") == 0) {
        set_state(app, DEMO_STATE_CRIT);
        return;
    }
    if (g_strcmp0(action_id, "rebuild-menu") == 0) {
        app->alternate_menu = !app->alternate_menu;
        rebuild_menu(app);
        return;
    }
    if (g_strcmp0(action_id, "print-info") == 0) {
        print_exporter_info(app);
        return;
    }
    if (g_strcmp0(action_id, "toggle-verbose") == 0) {
        app->verbose = !app->verbose;
        sni_exporter_menu_set_toggle_state(app->tray, "toggle-verbose", app->verbose);
        g_print("Verbose logging: %s\n", app->verbose ? "on" : "off");
        return;
    }
    if (g_strcmp0(action_id, "quit") == 0) {
        g_main_loop_quit(app->loop);
    }
}

int main(void) {
    GError *error = NULL;
    ExampleApp app = {0};

    app.loop = g_main_loop_new(NULL, FALSE);
    app.state = DEMO_STATE_NORMAL;
    app.tray = sni_exporter_new_with_menu_path("feature-demo", "/DemoMenu");

    sni_exporter_set_action_callback(app.tray, on_action, &app, NULL);
    sni_exporter_set_activate_callback(app.tray, on_activate, &app, NULL);
    sni_exporter_set_secondary_activate_callback(app.tray, on_secondary_activate, &app, NULL);
    sni_exporter_set_scroll_callback(app.tray, on_scroll, &app, NULL);
    rebuild_menu(&app);

    if (!sni_exporter_start(app.tray, &error)) {
        g_printerr("Unable to start tray exporter: %s\n", error->message);
        g_clear_error(&error);
        sni_exporter_free(app.tray);
        g_main_loop_unref(app.loop);
        return 1;
    }

    print_exporter_info(&app);
    update_live_info(&app);
    g_timeout_add_seconds(1, on_tick, &app);
    g_main_loop_run(app.loop);

    sni_exporter_stop(app.tray);
    sni_exporter_free(app.tray);
    g_main_loop_unref(app.loop);
    return 0;
}
