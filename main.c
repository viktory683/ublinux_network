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

// TODO split into smaller functions
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

        dev->ip4_addresses = g_ptr_array_new();
        int ip4_address_index = 1;
        char* ip4_address;
        do {
            char key[16] = "ip4_address_";
            char str_index[2];
            sprintf(str_index, "%d", ip4_address_index);
            strcat(key, str_index);
            ip4_address = parse_string(data, key);
            if (ip4_address) {
                g_ptr_array_add(dev->ip4_addresses, ip4_address);
            }
            ip4_address_index++;
        } while (ip4_address);

        dev->ip4_dnses = g_ptr_array_new();
        int ip4_dns_index = 1;
        char* ip4_dns;
        do {
            char key[16] = "ip4_dns_";
            char str_index[2];
            sprintf(str_index, "%d", ip4_dns_index);
            strcat(key, str_index);
            ip4_dns = parse_string(data, key);
            if (ip4_dns) {
                g_ptr_array_add(dev->ip4_dnses, ip4_dns);
            }
            ip4_dns_index++;
        } while (ip4_dns);

        dev->ip4_routes = g_ptr_array_new();
        int ip4_route_index = 1;
        json_t* ip4_route;
        do {
            char key[16] = "ip4_route_";
            char str_index[2];
            sprintf(str_index, "%d", ip4_route_index);
            strcat(key, str_index);
            ip4_route = json_object_get(data, key);
            if (ip4_route) {
                struct IPRoute* ip_route = malloc(sizeof(struct IPRoute));
                ip_route->dst = parse_string(ip4_route, "dst");
                ip_route->nh = parse_string(ip4_route, "nh");
                parse_int(ip4_route, "mt", &(ip_route->mt));
                g_ptr_array_add(dev->ip4_routes, ip_route);
            }
            ip4_route_index++;
        } while (ip4_route);

        dev->ip6_addresses = g_ptr_array_new();
        int ip6_address_index = 1;
        char* ip6_address;
        do {
            char key[16] = "ip6_address_";
            char str_index[2];
            sprintf(str_index, "%d", ip6_address_index);
            strcat(key, str_index);
            ip6_address = parse_string(data, key);
            if (ip6_address) {
                g_ptr_array_add(dev->ip6_addresses, ip6_address);
            }
            ip6_address_index++;

        } while (ip6_address);

        dev->ip6_routes = g_ptr_array_new();
        int ip6_route_index = 1;
        json_t* ip6_route;
        do {
            char key[16] = "ip6_route_";
            char str_index[2];
            sprintf(str_index, "%d", ip6_route_index);
            strcat(key, str_index);
            ip6_route = json_object_get(data, key);
            if (ip6_route) {
                struct IPRoute* ip_route = malloc(sizeof(struct IPRoute));
                ip_route->dst = parse_string(ip6_route, "dst");
                ip_route->nh = parse_string(ip6_route, "nh");
                parse_int(ip6_route, "mt", &(ip_route->mt));
                g_ptr_array_add(dev->ip6_routes, ip_route);
            }
            ip6_route_index++;
        } while (ip6_route);

        g_ptr_array_add(*devices, dev);
    }
}

void get_devices(GPtrArray** devices) {
    char* output = NULL;
    executeCommand("nmcli device show | jc --nmcli", &output);

    parse_devices(parse_json(output), devices);

    free(output);
}

char* validIpSymbols = "1234567890.";

void ipAddressChanged(GtkEntry* entry, gpointer user_data) {
    const char* text = gtk_entry_get_text(entry);
    printf("text: %s\n", text);

    bool wrongSymbols = false;

    for (int i = 0; i < strlen(text); i++) {
        if (!strchr(validIpSymbols, text[i])) {
            wrongSymbols = true;
            break;
        }
    }

    // 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14
    // 1  2  3  .  1  2  3  .  1  2  3  .  1  2  3
    // TODO octets can be less than 100 and dump check by indexes not working then
    bool octetsPoints = false;
    // for (int i = 3; i < ipLen; i += 4) {
    //     if (text[i] != '.') {
    //         octetsPoints = true;
    //         break;
    //     }
    // }

    if (octetsPoints || wrongSymbols) {
        if (octetsPoints) {
            gtk_label_set_text(ipAddressErrorText,
                               "IP address consists of four octets separated by a dot\nExample: 192.168.0.1");
        }

        if (wrongSymbols) {
            gtk_label_set_text(
                ipAddressErrorText,
                "Invalid Symbol\nIP address can contain only digits and a point ([0-9] and '.')\nExample: 192.168.0.1");
        }

        gtk_popover_popup(ipAddressErrorPopup);
        return;
    }

    struct in_addr addr;
    addr.s_addr = inet_addr(text);

    char ip_address[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(addr.s_addr), ip_address, INET_ADDRSTRLEN) == NULL) {
        fprintf(stderr, "Invalid IP address");
        return;
    }
    printf("IP: %s\n", ip_address);
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
        g_warning(err->message);
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
