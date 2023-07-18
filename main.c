#include "handlers.h"
#include <arpa/inet.h>
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

int safe_check() {
    char* commands[] = { "nmcli", "jc" };
    int commands_count = sizeof(commands) / sizeof(commands[0]);

    bool error = false;
    for (int i = 0; i < commands_count; i++) {
        char command[sizeof("which  > /dev/null 2>&1") + sizeof(commands[i])] = "which ";
        strcat(command, commands[i]);
        strcat(command, " > /dev/null 2>&1");

        if (system(command)) {
            error = true;
            fprintf(stderr, "command: '%s' is not found in system\n", commands[i]);
        }
    }
    return error;
}

/**
 * Executes a system command and stores the command output in a dynamically allocated memory.
 *
 * Written with phind (https://www.phind.com/agent?cache=clk6itjk4005jl708blm5tvza)
 *
 * @param command The command to execute.
 * @param output A pointer to a character pointer where the command output will be stored.
 *               The memory for the output will be dynamically allocated and should be freed by the caller.
 */
void executeCommand(const char* command, char** output) {
    FILE* fp;
    char buffer[1024];
    size_t outputSize = 0;
    size_t bufferSize = sizeof(buffer);

    fp = popen(command, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to execute command\n");
        return;
    }

    *output = NULL;

    while (fgets(buffer, bufferSize, fp) != NULL) {
        size_t lineSize = strlen(buffer);

        char* newOutput = realloc(*output, outputSize + lineSize + 1);
        if (newOutput == NULL) {
            fprintf(stderr, "Failed to allocate memory\n");
            free(*output);
            *output = NULL;
            break;
        }

        *output = newOutput;
        memcpy(*output + outputSize, buffer, lineSize);
        outputSize += lineSize;
    }

    if (*output != NULL) {
        (*output)[outputSize] = '\0';
    }

    pclose(fp);
}

json_t* parse_json(const char* text) {
    json_t* root;
    json_error_t error;
    root = json_loads(text, 0, &error);

    if (!root) {
        fprintf(stderr, "Error: on line %d: %s\n", error.line, error.text);
        return NULL;
    }

    return root;
}

void parse_devices(json_t* root, char*** devices, int* devices_count) {
    if (!json_is_array(root)) {
        fprintf(stderr, "error: root is not an array\n");
        json_decref(root);
        return;
    }

    *devices = malloc(json_array_size(root) * sizeof(char*));
    *devices_count = (int)json_array_size(root);

    for (int i = 0; i < json_array_size(root); i++) {
        json_t *data, *device;
        //, *type, *hwaddr, *mtu, *state, *state_text, *connection, *con_path, *ip4_address_1, *ip4_gateway,
        //*ip4_route_1, *ip4_route_2, *ip4_dns_1, *ip6_address_1, *ip6_gateway, *ip6_route_1;

        data = json_array_get(root, i);
        if (!json_is_object(data)) {
            fprintf(stderr, "error: commit data %d is not an object\n", i + 1);
            json_decref(root);
            return;
        }

        device = json_object_get(data, "device");
        if (!json_is_string(device)) {
            fprintf(stderr, "error: commit %d: sha is not a string\n", i + 1);
            json_decref(root);
            return;
        }

        (*devices)[i] = json_string_value(device);
    }
}

void get_devices(char*** devices, int* devices_count) {
    char* output = NULL;
    executeCommand("nmcli device show | jc --nmcli", &output);

    parse_devices(parse_json(output), devices, devices_count);

    free(output);
}

char* validIpSymbols = "1234567890.";

void ipAddressChanged(GtkEntry* entry, gpointer user_data) {
    char* text = gtk_entry_get_text(entry);
    printf("text: %s\n", text);

    int ipLen = strlen(text);
    bool wrongSymbols = false;

    for (int i = 0; i < ipLen; i++) {
        if (!strchr(validIpSymbols, text[i])) {
            wrongSymbols = true;
            break;
        }
    }

    // 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14
    // 1  2  3  .  1  2  3  .  1  2  3  .  1  2  3
    // TODO octets can be less than 100 and dump check by indexes not working
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

        gtk_widget_show(ipAddressErrorPopup);
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

    g_signal_connect(G_OBJECT(deviceBox), "changed", deviceBoxHandler, NULL);

    g_signal_connect(G_OBJECT(ipAddress), "changed", ipAddressChanged, NULL);

    char** devices;
    int devices_count;
    get_devices(&devices, &devices_count);
    updateComboBoxItems(deviceBox, devices_count, devices);

    for (int i = 0; i < devices_count; i++) {
        free(devices[i]);
    }
    free(devices);

    // freeing up memory
    g_object_unref(G_OBJECT(builder));

    gtk_widget_show(topWindow);

    gtk_main();

    return 0;
}

// TODO:
//     * update device setting on focus-out-event from ipAddress