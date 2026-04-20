[CCode(cheader_filename = "sni-exporter.h")]
namespace SniExporter {

    [CCode(cname = "SniExporterActionCallback", has_target = true)]
    public delegate void ActionCallback(string action_id);

    [CCode(cname = "SniExporterActivateCallback", has_target = true)]
    public delegate void ActivateCallback(int x, int y);

    [CCode(cname = "SniExporterScrollCallback", has_target = true)]
    public delegate void ScrollCallback(int delta, string orientation);

    [Compact]
    [CCode(cname = "SniExporter", free_function = "sni_exporter_free")]
    public class Exporter {
        [CCode(cname = "sni_exporter_new")]
        public Exporter(string item_id);

        [CCode(cname = "sni_exporter_new_with_menu_path")]
        public Exporter.with_menu_path(string item_id, string menu_object_path);

        [CCode(cname = "sni_exporter_start")]
        public bool start() throws GLib.Error;

        [CCode(cname = "sni_exporter_stop")]
        public void stop();

        // introspection
        [CCode(cname = "sni_exporter_get_bus_name")]
        public unowned string? get_bus_name();

        [CCode(cname = "sni_exporter_get_item_id")]
        public unowned string? get_item_id();

        [CCode(cname = "sni_exporter_get_menu_path")]
        public unowned string? get_menu_path();

        // properties
        [CCode(cname = "sni_exporter_set_title")]
        public void set_title(string title);

        [CCode(cname = "sni_exporter_set_status")]
        public void set_status(string status);

        [CCode(cname = "sni_exporter_set_tooltip")]
        public void set_tooltip(string title, string body);

        [CCode(cname = "sni_exporter_set_icon_argb")]
        public void set_icon_argb(int width, int height, [CCode(array_length_type = "gsize")] uint8[] argb);

        [CCode(cname = "sni_exporter_set_icon_name")]
        public void set_icon_name(string icon_name);

        [CCode(cname = "sni_exporter_set_icon_theme_path")]
        public void set_icon_theme_path(string path);

        [CCode(cname = "sni_exporter_set_item_is_menu")]
        public void set_item_is_menu(bool value);

        [CCode(cname = "sni_exporter_set_attention_icon_argb")]
        public void set_attention_icon_argb(int width, int height,
                                            [CCode(array_length_type = "gsize")] uint8[] argb);

        [CCode(cname = "sni_exporter_set_attention_icon_name")]
        public void set_attention_icon_name(string icon_name);

        // callbacks
        [CCode(cname = "sni_exporter_set_action_callback")]
        public void set_action_callback(owned ActionCallback? callback);

        [CCode(cname = "sni_exporter_set_activate_callback")]
        public void set_activate_callback(owned ActivateCallback? callback);

        [CCode(cname = "sni_exporter_set_secondary_activate_callback")]
        public void set_secondary_activate_callback(owned ActivateCallback? callback);

        [CCode(cname = "sni_exporter_set_scroll_callback")]
        public void set_scroll_callback(owned ScrollCallback? callback);

        // menu
        [CCode(cname = "sni_exporter_menu_begin")]
        public void menu_begin();

        [CCode(cname = "sni_exporter_menu_add_info_item")]
        public void menu_add_info_item(string label);

        [CCode(cname = "sni_exporter_menu_add_separator")]
        public void menu_add_separator();

        [CCode(cname = "sni_exporter_menu_add_action_item")]
        public void menu_add_action_item(string action_id, string label, string? icon_name);

        [CCode(cname = "sni_exporter_menu_add_toggle_item")]
        public void menu_add_toggle_item(string action_id, string label,
                                         string? icon_name, bool checked);

        [CCode(cname = "sni_exporter_menu_set_toggle_state")]
        public void menu_set_toggle_state(string action_id, bool checked);

        [CCode(cname = "sni_exporter_menu_end")]
        public void menu_end();

        [CCode(cname = "sni_exporter_menu_update_label")]
        public void menu_update_label(string action_id, string new_label);
    }
}
