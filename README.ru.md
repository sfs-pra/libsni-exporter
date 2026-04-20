# libsni-exporter

Небольшая библиотека на чистом C поверх GLib/GIO, которая экспортирует
`org.kde.StatusNotifierItem` и классическое меню `com.canonical.dbusmenu`
через D-Bus.

Код вынесен из прямой SNI-реализации в `sensindicator` без зависимости от
`libayatana-appindicator`.

## Что умеет

- прямой экспорт SNI в session bus
- прямой экспорт dbusmenu в session bus
- простой C API для item + menu
- shared library и pkg-config metadata

## Текущее покрытие

Реализовано:

- обновление title / status / tooltip
- установка ARGB pixmap-иконки (Cairo BGRA -> ARGB-BE конвертация)
- иконка по имени из темы
- путь к теме иконок
- attention icon по ARGB pixmap или имени из темы
- callback'и activate / secondary activate / scroll
- простое плоское меню с разделителями
- неактивные info items
- callback на активацию пунктов меню
- live-обновление label (зависит от host)
- полная перестройка меню

Пока не реализовано:

- submenus
- check/radio items
- overlay icon

## Сборка

```bash
meson setup build --prefix=/usr
meson compile -C build
```

## Стиль API

Библиотека отдает непрозрачный handle `SniExporter` с функциями для:

- запуска и остановки tray-export в session bus
- установки title, status и tooltip
- установки иконки по ARGB pixmap или по имени из темы
- обработки левого клика, среднего клика и прокрутки
- построения простого flat dbusmenu
- обработки активации menu actions

## Пример

В пакет устанавливается демонстрационный C-пример:

`/usr/share/doc/libsni-exporter/minimal-tray.c`

и соответствующий `Makefile`:

`/usr/share/doc/libsni-exporter/Makefile`

Типичная сборка примера:

```bash
cc minimal-tray.c $(pkg-config --cflags --libs sni-exporter gio-2.0 glib-2.0) -o minimal-tray
```

или просто:

```bash
make
```

Что показывает demo:

- `sni_exporter_new_with_menu_path()`
- `sni_exporter_start()` / `sni_exporter_stop()`
- `sni_exporter_get_item_id()` / `get_menu_path()` / `get_bus_name()`
- динамическое обновление title / status / tooltip
- динамическую смену ARGB-иконки для состояний normal / warn / crit
- построение flat-меню с info items, actions и separators
- `sni_exporter_menu_update_label()` для живых пунктов меню
- полную перестройку меню через `menu_begin()` / `menu_end()`
- обработку action callback из экспортированного dbusmenu

## Сравнение с libayatana-appindicator

| Возможность | libayatana-appindicator | libsni-exporter |
|---|---|---|
| Иконка по имени темы | да | **да** |
| Иконка ARGB pixmap | да | **да** |
| Путь к теме иконок | да | **да** |
| Title / Status / Tooltip | да | **да** |
| Плоское меню с действиями | да (через GtkMenu) | **да** (встроенное) |
| Разделители в меню | да | **да** |
| Неактивные info items | да | **да** |
| Callback на действия меню | да | **да** |
| Полная перестройка меню | да | **да** |
| Live-обновление label в меню | да | **да** (зависит от host) |
| Activate callback (левый клик) | да | **да** |
| SecondaryActivate (средний клик) | да | **да** |
| Scroll callback | да | **да** |
| Submenus | да | пока нет |
| Check/radio items | да | пока нет |
| Attention icon | да | **да** |
| Overlay icon | да | пока нет |
| XAyatanaLabel | да | пока нет |
| Интеграция с GtkMenu | да | нет (свой menu API) |
| GObject type system | да | нет (opaque C struct) |
| X11 GtkStatusIcon fallback | да | нет (фокус на Wayland) |
| Зависимости | gtk3 + dbusmenu-glib | только GLib/GIO |

## Формат ARGB pixmap

Протокол SNI ожидает пиксели в порядке **ARGB big-endian**:
`byte[0]=A, byte[1]=R, byte[2]=G, byte[3]=B` на каждый пиксель.

Cairo (`CAIRO_FORMAT_ARGB32`) хранит пиксели в native byte order — на
little-endian (x86) системах это **BGRA**. Необходима конвертация:

```c
argb_be[dst + 0] = cairo_raw[src + 3];  /* A */
argb_be[dst + 1] = cairo_raw[src + 2];  /* R */
argb_be[dst + 2] = cairo_raw[src + 1];  /* G */
argb_be[dst + 3] = cairo_raw[src + 0];  /* B */
```

Полный рабочий пример — см. `minimal-tray.c`.

## Поведение ItemIsMenu

По умолчанию библиотека устанавливает `ItemIsMenu=true`. Вызовите
`sni_exporter_set_item_is_menu(tray, FALSE)` до `sni_exporter_start()`, чтобы включить
callback `Activate` по левому клику на хостах, которые это поддерживают (Waybar, swaybar).
Примечание: sfwbar всегда показывает меню независимо от этого флага.

## Быстрый старт

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

Сборка: `cc app.c $(pkg-config --cflags --libs sni-exporter gio-2.0 glib-2.0) -o app`

## Назначение

Библиотека рассчитана на проекты, которым нужен небольшой прямой SNI/dbusmenu
exporter на wlroots/KDE-совместимых host'ах без подключения
`libayatana-appindicator`.

Типичные кандидаты: мониторы датчиков, индикаторы батареи, простые статусные апплеты.

## Совместимые SNI-хосты

| Хост | Платформа | Левый клик | Примечания |
|---|---|---|---|
| **sfwbar** | Wayland | Открывает меню | Игнорирует `ItemsPropertiesUpdated` — использовать `menu_begin()`/`menu_end()` |
| **Waybar** | Wayland | `Activate` или меню если `ItemIsMenu=true` | Зависит от `libdbusmenu-gtk3` |
| **swaybar** (Sway) | Wayland | Учитывает `ItemIsMenu` | Использует `sd-bus` напрямую |
| **Ironbar** | Wayland | Открывает меню | Абсолютные пути к иконкам не работают — использовать имена из темы |
| **Quickshell** | Wayland + X11 | `activate()` | QML/Qt |
| **nwg-panel** | Wayland | — | GTK3/Python |
| **KDE Plasma** | Wayland + X11 | Учитывает `ItemIsMenu` | Эталонная реализация |
| **LXQt panel** | X11 | `Activate` | Нативно с LXQt 0.10.0 |
| **XFCE4 panel** | X11 | `Activate` | Нативно с XFCE 4.15 (встроено в systray) |
| **GNOME Shell** | Wayland + X11 | — | Требует расширение [#615](https://extensions.gnome.org/extension/615/) |
| **Cinnamon** | X11 | — | Нативно с 2.8 |
| **MATE panel** | X11 | — | Нативно с 1.18 |
| **i3 / openbox** | X11 | — | Через мост [snixembed](https://sr.ht/~steef/snixembed/) |

### Конфликт StatusNotifierWatcher

Одновременно может работать только один `org.kde.StatusNotifierWatcher`. Если `kded6` (демон KDE) стартует раньше вашей панели — он перехватывает watcher и иконки пропадают. Решение: `pkill kded6`.
