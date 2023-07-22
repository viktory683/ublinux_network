#include "helpers.h"
#include "json_helpers.h"
#include <arpa/inet.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <jansson.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define UI_FILE "design.glade"

GtkBuilder* builder;
GtkWidget* topWindow;

GtkComboBoxText* interfaceComboBox;

GtkEntry* ipAddressEntry;
GtkPopover* ipAddressPopover;
GtkLabel* ipAddressPopoverLabel;

GPtrArray* interfaces;

void update_combo_box_items(GtkComboBoxText* box, int itemc, char** itemv) {
    for (int i = 0; i < itemc; i++) {
        gtk_combo_box_text_append(box, NULL, itemv[i]);
    }

    gtk_combo_box_set_active(GTK_COMBO_BOX(box), 0);
}

void update_interface_combo_box(GtkComboBoxText* box, GPtrArray* interfaces) {
    char** devices = malloc(interfaces->len * sizeof(char*));

    for (int i = 0; i < interfaces->len; i++) {
        struct Device* dev = interfaces->pdata[i];
        devices[i] = dev->device;
    }

    update_combo_box_items(box, interfaces->len, devices);

    free(devices);
}

void get_devices(GPtrArray** devices) {
    char* output = NULL;
    execute_command("nmcli device show | jc --nmcli", &output);

    json_parse_devices(parse_json(output), devices);

    free(output);
}

int is_ip_valid(const char* ip_address) {
    // 1 | invalid symbols
    char* validSymbols = "1234567890.";
    for (int i = 0; i < strlen(ip_address); i++) {
        if (!strchr(validSymbols, ip_address[i])) {
            return 1;
        }
    }

    // 2 | octets != 4
    int point_count = 0;
    for (int i = 0; i < strlen(ip_address); i++) {
        if (ip_address[i] == '.') {
            point_count++;
        }
    }
    if (point_count != 3) {
        return 2;
    }

    // 3 | 0 <= octet < 256
    int octets[4];
    sscanf(ip_address, "%d.%d.%d.%d", &octets[0], &octets[1], &octets[2], &octets[3]);
    for (int i = 0; i < 4; i++) {
        if (octets[i] < 0 || octets[i] > 255) {
            return 3;
        }
    }

    return 0;
}

// ======== HANDLERS ========

G_MODULE_EXPORT void on_interface_combo_box_changed(GtkComboBoxText* box, gpointer user_data) {
    char* choosen = gtk_combo_box_text_get_active_text(box);

    // TODO update UI fields (sensitive etc) based on device info (dhcp/ip/mask etc)
    printf("choosen device: '%s'\n", choosen);
}

G_MODULE_EXPORT void on_ip_address_entry_changed(GtkEntry* entry, gpointer user_data) {
    const char* text = gtk_entry_get_text(entry);
    printf("text: %s\n", text);
}

G_MODULE_EXPORT gboolean on_ip_address_entry_focus_out(GtkWidget* entry, GdkEventFocus event, gpointer user_data) {
    const char* ip_address = gtk_entry_get_text(ipAddressEntry);

    char* errorMessage = NULL;
    switch (is_ip_valid(ip_address)) {
    case 1:
        errorMessage = "IP address can only contain numbers and dots\nExample: 192.168.0.1";
        break;
    case 2:
        errorMessage = "IP address must consist of four octets";
        break;
    case 3:
        errorMessage = "The octets of an IP address can only contain numbers in the range from 0 to 255";
        break;
    }
    if (errorMessage) {
        gtk_label_set_text(ipAddressPopoverLabel, errorMessage);
        gtk_popover_popup(ipAddressPopover);
        return false;
    }

    // TODO change ip address of interface with nmcli
    printf("IP: %s\n", ip_address);

    return true;
}

G_MODULE_EXPORT void onExit(GtkWidget* w) {
    printf("Exit");

    for (int i = 0; i < interfaces->len; i++) {
        struct Device* dev = g_ptr_array_index(interfaces, i);
        g_ptr_array_free(dev->ip4_addresses, true);
        g_ptr_array_free(dev->ip4_dnses, true);
        g_ptr_array_free(dev->ip4_routes, true);
        g_ptr_array_free(dev->ip6_addresses, true);
        g_ptr_array_free(dev->ip6_routes, true);
    }
    g_ptr_array_free(interfaces, true);

    gtk_main_quit();
}

// =========================

int main(int argc, char* argv[]) {
    if (safe_check()) {
        fprintf(stderr, "Some commands are not found in system\n");
        fprintf(stderr, "Please install them and try again\n");
        return 1;
    }

    GError* err = NULL;

    // GTK+ initialization
    gtk_init(&argc, &argv);

    builder = gtk_builder_new();

    // * UI file should be placed to the same directory as the application
    if (!gtk_builder_add_from_file(builder, UI_FILE, &err)) {
        g_warning("%s\n", err->message);
        g_free(err);
        return 1;
    }

    topWindow = GTK_WIDGET(gtk_builder_get_object(builder, "topWindow"));

    interfaceComboBox = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "interfaceComboBox"));

    ipAddressEntry = GTK_ENTRY(gtk_builder_get_object(builder, "ipAddressEntry"));
    ipAddressPopover = GTK_POPOVER(gtk_builder_get_object(builder, "ipAddressPopover"));
    ipAddressPopoverLabel = GTK_LABEL(gtk_builder_get_object(builder, "ipAddressPopoverLabel"));

    get_devices(&interfaces);

    update_interface_combo_box(interfaceComboBox, interfaces);

    gtk_builder_connect_signals(builder, NULL);

    // freeing up memory
    g_object_unref(G_OBJECT(builder));

    gtk_widget_show_all(topWindow);

    gtk_main();

    return 0;
}
