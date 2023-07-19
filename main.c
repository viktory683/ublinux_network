#include "handlers.h"
#include "helpers.h"
#include <arpa/inet.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <jansson.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define UI_FILE "design.glade"

GtkBuilder* builder;
GtkWindow* topWindow;

GtkComboBoxText* deviceBox;

GtkEntry* ipAddress;
GtkPopover* ipAddressErrorPopup;
GtkLabel* ipAddressErrorText;

void updateComboBoxItems(GtkComboBoxText* box, int itemc, char** items) {
    for (int i = 0; i < itemc; i++) {
        gtk_combo_box_text_append(box, NULL, items[i]);
    }

    gtk_combo_box_set_active(GTK_COMBO_BOX(box), 0);
}

struct IPRoute {
    char* dst;
    char* nh;
    int mt;
};

struct Device {
    char* device;
    char* type;
    char* hwaddr;
    char* state_text;
    char* connection;
    char* con_path;
    char* ip4_gateway;
    char* ip6_gateway;
    int mtu;
    int state;
    GPtrArray* ip4_addresses; // str
    GPtrArray* ip4_dnses;     // str
    GPtrArray* ip4_routes;    // IPRoute
    GPtrArray* ip6_addresses; // str
    GPtrArray* ip6_routes;    // IPRoute
};

char* parse_string(json_t* object, const char* key) {
    if (!json_is_object(object)) {
        fprintf(stderr, "error: object is not a json object\n");
        // json_decref(object);
        return NULL;
    }

    json_t* value = json_object_get(object, key);
    if (!json_is_string(value)) {
        fprintf(stderr, "error: %s is not a string\n", key);
        // json_decref(object);
        return NULL;
    }

    return json_string_value(value);
}

void parse_int(json_t* object, const char* key, int* value) {
    if (!json_is_object(object)) {
        fprintf(stderr, "error: object is not a json object\n");
        // json_decref(object);
        // return NULL;
    }

    json_t* value_t = json_object_get(object, key);
    if (!json_is_integer(value_t)) {
        fprintf(stderr, "error: %s is not a integer\n", key);
        // json_decref(object);
        // return NULL;
    }

    *value = json_integer_value(value_t);
}

GPtrArray* parse_string_array(json_t* obj, const char* key_prefix, int start) {
    GPtrArray* array = g_ptr_array_new();

    char* value;
    do {
        char key[32];
        strcpy(key, key_prefix);
        char str_index[2];
        sprintf(str_index, "%d", start);
        strcat(key, str_index);
        value = parse_string(obj, key);
        if (value) {
            g_ptr_array_add(array, value);
        }
        start++;
    } while (value);

    return array;
}

GPtrArray* parse_IPRoute_array(json_t* obj, const char* key_prefix, int start) {
    GPtrArray* array = g_ptr_array_new();

    json_t* value;
    do {
        char key[32];
        strcpy(key, key_prefix);
        char str_index[2];
        sprintf(str_index, "%d", start);
        strcat(key, str_index);
        value = json_object_get(obj, key);
        if (value) {
            struct IPRoute* ip_route = malloc(sizeof(struct IPRoute));
            ip_route->dst = parse_string(value, "dst");
            ip_route->nh = parse_string(value, "nh");
            parse_int(value, "mt", &(ip_route->mt));
            g_ptr_array_add(array, ip_route);
        }
        start++;
    } while (value);

    return array;
}

void parse_devices(json_t* root, GPtrArray** devices) {
    if (!json_is_array(root)) {
        fprintf(stderr, "error: root is not an array\n");
        json_decref(root);
        return;
    }

    *devices = g_ptr_array_new();

    for (int i = 0; i < json_array_size(root); i++) {
        json_t* data = json_array_get(root, i);
        if (!json_is_object(data)) {
            fprintf(stderr, "error: commit data %d is not an object\n", i + 1);
            json_decref(root);
            return;
        }

        struct Device* dev = malloc(sizeof(struct Device));

        dev->device = parse_string(data, "device");
        dev->type = parse_string(data, "type");
        dev->hwaddr = parse_string(data, "hwaddr");
        parse_int(data, "mtu", &(dev->mtu));
        parse_int(data, "state", &(dev->state));
        dev->state_text = parse_string(data, "state_text");
        dev->connection = parse_string(data, "connection");
        dev->con_path = parse_string(data, "con_path");
        dev->ip4_gateway = parse_string(data, "ip4_gateway");
        dev->ip6_gateway = parse_string(data, "ip6_gateway");

        dev->ip4_addresses = parse_string_array(data, "ip4_address_", 1);
        dev->ip4_dnses = parse_string_array(data, "ip4_dns_", 1);
        dev->ip4_routes = parse_IPRoute_array(data, "ip4_route_", 1);
        dev->ip6_addresses = parse_string_array(data, "ip6_address_", 1);
        dev->ip6_routes = parse_IPRoute_array(data, "ip6_route_", 1);

        g_ptr_array_add(*devices, dev);
    }
}

void get_devices(GPtrArray** devices) {
    char* output = NULL;
    executeCommand("nmcli device show | jc --nmcli", &output);

    parse_devices(parse_json(output), devices);

    free(output);
}

int isIPValid(const char* ip_address) {
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

void ipAddressChanged(GtkEntry* entry, gpointer user_data) {
    const char* text = gtk_entry_get_text(entry);
    printf("text: %s\n", text);
}

gboolean ipAddressEntered(GtkWidget* entry, GdkEventFocus event, gpointer user_data) {
    const char* ip_address = gtk_entry_get_text(ipAddress);

    char* errorMessage = NULL;
    switch (isIPValid(ip_address)) {
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
        gtk_label_set_text(ipAddressErrorText, errorMessage);
        gtk_popover_popup(ipAddressErrorPopup);
        return false;
    }

    // TODO change ip address of interface with nmcli
    printf("IP: %s\n", ip_address);

    return true;
}

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

    topWindow = GTK_WINDOW(gtk_builder_get_object(builder, "topWindow"));

    deviceBox = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "deviceBox"));

    ipAddress = GTK_ENTRY(gtk_builder_get_object(builder, "ipAddress"));
    ipAddressErrorPopup = GTK_POPOVER(gtk_builder_get_object(builder, "ipAddressPopup"));
    ipAddressErrorText = GTK_LABEL(gtk_builder_get_object(builder, "ipAddressErrorText"));

    g_signal_connect(G_OBJECT(topWindow), "destroy", gtk_main_quit, NULL);

    g_signal_connect(G_OBJECT(deviceBox), "changed", G_CALLBACK(deviceBoxHandler), NULL);

    g_signal_connect(G_OBJECT(ipAddress), "changed", G_CALLBACK(ipAddressChanged), NULL);
    g_signal_connect(G_OBJECT(ipAddress), "focus-out-event", G_CALLBACK(ipAddressEntered), NULL);

    GPtrArray* devices;
    get_devices(&devices);
    if (devices->len == 0) {
        fprintf(stderr, "No devices");
        return 0;
    }

    char** device_names = malloc(devices->len * sizeof(char*));
    for (int i = 0; i < devices->len; i++) {
        struct Device* dev = g_ptr_array_index(devices, i);
        device_names[i] = dev->device;
    }

    updateComboBoxItems(deviceBox, devices->len, device_names);

    // freeing up memory
    g_object_unref(G_OBJECT(builder));

    gtk_widget_show(GTK_WIDGET(topWindow));

    gtk_main();

    for (int i = 0; i < devices->len; i++) {
        free(device_names[i]);
    }
    free(device_names);

    for (int i = 0; i < devices->len; i++) {
        struct Device* dev = g_ptr_array_index(devices, i);
        g_ptr_array_free(dev->ip4_addresses, true);
        g_ptr_array_free(dev->ip4_dnses, true);
        g_ptr_array_free(dev->ip4_routes, true);
        g_ptr_array_free(dev->ip6_addresses, true);
        g_ptr_array_free(dev->ip6_routes, true);
    }
    g_ptr_array_free(devices, true);

    return 0;
}

// TODO:
//     * update device setting on focus-out-event from ipAddress
