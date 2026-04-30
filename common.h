
//Oskar Kalinowski 352818
#pragma once
#include <cstdint>

struct [[gnu::packed]] InterfaceInfo {
    uint32_t ip_net;
    uint8_t mask;
    uint32_t distance;
};

struct [[gnu::packed]] RoutingPacket {
    uint32_t ip_net;
    uint8_t mask;
    uint32_t distance;
};

struct RouteEntry {
    uint32_t network;
    uint8_t mask;
    uint32_t distance;
    uint32_t via_ip;
    uint64_t last_seen;
    uint32_t direct_distance;
    bool is_direct;
    bool removed;
};
