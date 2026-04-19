#include "routing_table.h"

#include <arpa/inet.h>
#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

uint32_t ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    uint32_t host = (static_cast<uint32_t>(a) << 24u) |
                    (static_cast<uint32_t>(b) << 16u) |
                    (static_cast<uint32_t>(c) << 8u) |
                    static_cast<uint32_t>(d);
    return htonl(host);
}

RouteEntry route(uint32_t distance, uint32_t via_ip, uint64_t last_seen, bool is_direct) {
    RouteEntry r{};
    r.distance = distance;
    r.via_ip = via_ip;
    r.last_seen = last_seen;
    r.is_direct = is_direct;
    return r;
}

void test_empty_lookup_returns_null() {
    RoutingTable table;
    assert(table.lookup(ip(10, 0, 0, 1)) == nullptr);
}

void test_exact_match_and_fields() {
    RoutingTable table;
    const RouteEntry expected = route(3, ip(192, 168, 1, 1), 100, false);

    table.insert(ip(10, 1, 2, 0), 24, expected);

    const RouteEntry* found = table.lookup(ip(10, 1, 2, 77));
    assert(found != nullptr);
    assert(found->distance == expected.distance);
    assert(found->via_ip == expected.via_ip);
    assert(found->last_seen == expected.last_seen);
    assert(found->is_direct == expected.is_direct);
}

void test_longest_prefix_match() {
    RoutingTable table;

    table.insert(ip(10, 0, 0, 0), 8, route(10, ip(1, 1, 1, 1), 1, false));
    table.insert(ip(10, 1, 0, 0), 16, route(5, ip(2, 2, 2, 2), 2, false));
    table.insert(ip(10, 1, 2, 0), 24, route(1, ip(3, 3, 3, 3), 3, true));

    const RouteEntry* a = table.lookup(ip(10, 1, 2, 99));
    assert(a != nullptr);
    assert(a->distance == 1);

    const RouteEntry* b = table.lookup(ip(10, 1, 200, 1));
    assert(b != nullptr);
    assert(b->distance == 5);

    const RouteEntry* c = table.lookup(ip(10, 200, 1, 1));
    assert(c != nullptr);
    assert(c->distance == 10);
}

void test_default_route() {
    RoutingTable table;

    table.insert(ip(0, 0, 0, 0), 0, route(20, ip(9, 9, 9, 9), 10, false));

    const RouteEntry* found = table.lookup(ip(123, 45, 67, 89));
    assert(found != nullptr);
    assert(found->distance == 20);
}

void test_host_route_beats_prefix() {
    RoutingTable table;

    table.insert(ip(172, 16, 0, 0), 12, route(4, ip(7, 7, 7, 7), 1, false));
    table.insert(ip(172, 16, 5, 10), 32, route(0, 0, 2, true));

    const RouteEntry* exact = table.lookup(ip(172, 16, 5, 10));
    assert(exact != nullptr);
    assert(exact->distance == 0);

    const RouteEntry* nearby = table.lookup(ip(172, 16, 5, 11));
    assert(nearby != nullptr);
    assert(nearby->distance == 4);
}

void test_update_existing_prefix() {
    RoutingTable table;

    table.insert(ip(192, 168, 0, 0), 16, route(2, ip(1, 1, 1, 1), 1, false));
    table.insert(ip(192, 168, 0, 0), 16, route(9, ip(8, 8, 8, 8), 99, true));

    const RouteEntry* found = table.lookup(ip(192, 168, 77, 88));
    assert(found != nullptr);
    assert(found->distance == 9);
    assert(found->via_ip == ip(8, 8, 8, 8));
    assert(found->last_seen == 99);
    assert(found->is_direct);
}

void test_clear_resets_table() {
    RoutingTable table;

    table.insert(ip(10, 10, 0, 0), 16, route(6, ip(1, 2, 3, 4), 7, false));
    assert(table.lookup(ip(10, 10, 1, 1)) != nullptr);

    table.clear();

    assert(table.lookup(ip(10, 10, 1, 1)) == nullptr);
    assert(table.node_count() == 1);
}

}  // namespace

int main() {
    test_empty_lookup_returns_null();
    test_exact_match_and_fields();
    test_longest_prefix_match();
    test_default_route();
    test_host_route_beats_prefix();
    test_update_existing_prefix();
    test_clear_resets_table();

    std::cout << "All routing table tests passed.\n";
    return 0;
}
