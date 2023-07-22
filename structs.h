#include <glib.h>

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