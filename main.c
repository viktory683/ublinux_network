#include "handlers.h"
#include "helpers.h"
#include <arpa/inet.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <jansson.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
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
    GPtrArray* ip4_addresses; // st
    GPtrArray* ip4_dnses;     // str
    GPtrArray* ip4_route;     // IPRoute
    GPtrArray* ip6_address;   // str
    GPtrArray* ip6_route;     // IPRoute
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

int parse_int(json_t* object, const char* key) {
    if (!json_is_object(object)) {
        fprintf(stderr, "error: object is not a json object\n");
        // json_decref(object);
        return NULL;
    }

    json_t* value = json_object_get(object, key);
    if (!json_is_integer(value)) {
        fprintf(stderr, "error: %s is not a integer\n", key);
        // json_decref(object);
        return NULL;
    }

    return json_integer_value(value);
}

void parse_devices(json_t* root, int* devices_count, struct Device*** devices) {
    if (!json_is_array(root)) {
        fprintf(stderr, "error: root is not an array\n");
        json_decref(root);
        return;
    }

    (*devices_count) = (int)json_array_size(root);
    (*devices) = malloc(json_array_size(root) * sizeof(struct Device*));

    for (int i = 0; i < json_array_size(root); i++) {
        json_t* data = json_array_get(root, i);
        if (!json_is_object(data)) {
            fprintf(stderr, "error: commit data %d is not an object\n", i + 1);
            json_decref(root);
            return;
        }

        (*devices)[i] = malloc(sizeof(struct Device));

        (*devices)[i]->device = parse_string(data, "device");
        (*devices)[i]->type = parse_string(data, "type");
        (*devices)[i]->hwaddr = parse_string(data, "hwaddr");
        (*devices)[i]->mtu = parse_int(data, "mtu");
        (*devices)[i]->state = parse_int(data, "state");
        (*devices)[i]->state_text = parse_string(data, "state_text");
        (*devices)[i]->connection = parse_string(data, "connection");
        (*devices)[i]->con_path = parse_string(data, "con_path");
        (*devices)[i]->ip4_gateway = parse_string(data, "ip4_gateway");
        (*devices)[i]->ip6_gateway = parse_string(data, "ip6_gateway");

        (*devices)[i]->ip4_addresses = g_ptr_array_new();
        int ip4_address_index = 1;
        char* ip4_address;
        do {
            char key[16] = "ip4_address_";
            char str_index[2];
            sprintf(str_index, "%d", ip4_address_index);
            strcat(key, str_index);
            ip4_address = parse_string(data, key);
            if (ip4_address) {
                g_ptr_array_add((*devices)[i]->ip4_addresses, ip4_address);
            }
            ip4_address_index++;
            
        } while (ip4_address);

        (*devices)[i]->ip4_dnses = g_ptr_array_new();
        int ip4_dns_index = 1;
        char* ip4_dns;
        do {
            char key[16] = "ip4_dns_";
            char str_index[2];
            sprintf(str_index, "%d", ip4_dns_index);
            strcat(key, str_index);
            ip4_dns = parse_string(data, key);
            if (ip4_dns) {
                g_ptr_array_add((*devices)[i]->ip4_dnses, ip4_dns);
            }
            ip4_dns_index++;
            
        } while (ip4_dns);
    }
}

void get_devices(int* devices_count, struct Device*** devices) {
    char* output = NULL;
    executeCommand("nmcli device show | jc --nmcli", &output);

    parse_devices(parse_json(output), devices_count, devices);

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

    int devices_count;
    struct Device** devices;
    get_devices(&devices_count, &devices);
    if (devices == NULL) {
        fprintf(stderr, "No devices");
    }
    for (int i = 0; i < devices_count; i++) {
        printf("device: %s\n", devices[i]->device);
        printf("type: %s\n", devices[i]->type);
        printf("hwaddr: %s\n", devices[i]->hwaddr);
        printf("mtu: %d\n", devices[i]->mtu);
        printf("state: %d\n", devices[i]->state);
        printf("state_text: %s\n", devices[i]->state_text);
        printf("connection: %s\n", devices[i]->connection);
        printf("con_path: %s\n", devices[i]->con_path);
        // printf("ip4_address_1: %s\n", devices[i]->ip4_address_1);
        printf("ip4_gateway: %s\n", devices[i]->ip4_gateway);

        for (int j = 0; j < devices[i]->ip4_addresses->len; j++) {
            printf("ip4_address_%d: %s\n", j, g_ptr_array_index(devices[i]->ip4_addresses, j));
        }

        for (int j = 0; j < devices[i]->ip4_dnses->len; j++) {
            printf("ip4_dns_%d: %s\n", j, g_ptr_array_index(devices[i]->ip4_dnses, j));
        }
        // printf("ip4_route_1:\n");
        // printf("\tdst: %s\n", devices[i]->ip4_route_1.dst);
        // printf("\tnh: %s\n", devices[i]->ip4_route_1.nh);
        // printf("\tmt: %d\n", devices[i]->ip4_route_1.mt);
        printf("================================================================\n");
    }

    char** device_names = malloc(devices_count * sizeof(char*));
    for (int i = 0; i < devices_count; i++) {
        device_names[i] = devices[i]->device;
    }

    updateComboBoxItems(deviceBox, devices_count, device_names);

    for (int i = 0; i < devices_count; i++) {
        free(device_names[i]);
    }
    free(device_names);

    for (int i = 0; i < devices_count; i++) {
        free(devices[i]);
    }
    free(devices);

    // freeing up memory
    g_object_unref(G_OBJECT(builder));

    gtk_widget_show(GTK_WIDGET(topWindow));

    // gtk_main();

    return 0;
}

// TODO:
//     * update device setting on focus-out-event from ipAddress
