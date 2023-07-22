#include "structs.h"
#include <glib.h>
#include <jansson.h>

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

char* json_parse_string(json_t* object, const char* key) {
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

void json_parse_int(json_t* object, const char* key, int* value) {
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

GPtrArray* json_parse_string_array(json_t* obj, const char* key_prefix, int start) {
    GPtrArray* array = g_ptr_array_new();

    char* value;
    do {
        char key[32];
        strcpy(key, key_prefix);
        char str_index[2];
        sprintf(str_index, "%d", start);
        strcat(key, str_index);
        value = json_parse_string(obj, key);
        if (value) {
            g_ptr_array_add(array, value);
        }
        start++;
    } while (value);

    return array;
}

GPtrArray* json_parse_IPRoute_array(json_t* obj, const char* key_prefix, int start) {
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
            ip_route->dst = json_parse_string(value, "dst");
            ip_route->nh = json_parse_string(value, "nh");
            json_parse_int(value, "mt", &(ip_route->mt));
            g_ptr_array_add(array, ip_route);
        }
        start++;
    } while (value);

    return array;
}

void json_parse_devices(json_t* root, GPtrArray** devices) {
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

        dev->device = json_parse_string(data, "device");
        dev->type = json_parse_string(data, "type");
        dev->hwaddr = json_parse_string(data, "hwaddr");
        json_parse_int(data, "mtu", &(dev->mtu));
        json_parse_int(data, "state", &(dev->state));
        dev->state_text = json_parse_string(data, "state_text");
        dev->connection = json_parse_string(data, "connection");
        dev->con_path = json_parse_string(data, "con_path");
        dev->ip4_gateway = json_parse_string(data, "ip4_gateway");
        dev->ip6_gateway = json_parse_string(data, "ip6_gateway");

        dev->ip4_addresses = json_parse_string_array(data, "ip4_address_", 1);
        dev->ip4_dnses = json_parse_string_array(data, "ip4_dns_", 1);
        dev->ip4_routes = json_parse_IPRoute_array(data, "ip4_route_", 1);
        dev->ip6_addresses = json_parse_string_array(data, "ip6_address_", 1);
        dev->ip6_routes = json_parse_IPRoute_array(data, "ip6_route_", 1);

        g_ptr_array_add(*devices, dev);
    }
}