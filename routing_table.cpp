#include "routing_table.h"

#include <arpa/inet.h>

RoutingTable::RoutingTable() {
    nodes_.reserve(1024);
    nodes_.push_back({{INVALID_INDEX, INVALID_INDEX}, INVALID_INDEX});
}

void RoutingTable::insert(uint32_t network, uint8_t mask, const RouteEntry& data) {
    if (mask > 32) {
        return;
    }

    uint32_t host_order_net = ntohl(network);
    uint32_t node_index = 0;

    for (uint8_t bit = 0; bit < mask; ++bit) {
        uint32_t direction = bit_at(host_order_net, bit);
        uint32_t next = nodes_[node_index].child[direction];

        if (next == INVALID_INDEX) {
            next = static_cast<uint32_t>(nodes_.size());
            nodes_[node_index].child[direction] = next;
            nodes_.push_back({{INVALID_INDEX, INVALID_INDEX}, INVALID_INDEX});
        }

        node_index = next;
    }

    uint32_t route_index = nodes_[node_index].route_index;
    if (route_index == INVALID_INDEX) {
        RouteEntry stored = data;
        stored.network = network;
        stored.mask = mask;
        stored.removed = false;
        routes_.push_back(stored);
        nodes_[node_index].route_index = static_cast<uint32_t>(routes_.size() - 1);
        return;
    }

    RouteEntry stored = data;
    stored.network = network;
    stored.mask = mask;
    stored.removed = false;
    routes_[route_index] = stored;
}

RouteEntry* RoutingTable::lookup(uint32_t ip) {
    const RoutingTable* self = this;
    return const_cast<RouteEntry*>(self->lookup(ip));
}

RouteEntry* RoutingTable::lookup_exact(uint32_t network, uint8_t mask) {
    const RoutingTable* self = this;
    return const_cast<RouteEntry*>(self->lookup_exact(network, mask));
}

const RouteEntry* RoutingTable::lookup_exact(uint32_t network, uint8_t mask) const {
    if (mask > 32) {
        return nullptr;
    }

    uint32_t host_order_net = ntohl(network);
    uint32_t node_index = 0;

    for (uint8_t bit = 0; bit < mask; ++bit) {
        uint32_t direction = bit_at(host_order_net, bit);
        uint32_t next = nodes_[node_index].child[direction];
        if (next == INVALID_INDEX) {
            return nullptr;
        }

        node_index = next;
    }

    uint32_t route_index = nodes_[node_index].route_index;
    if (route_index == INVALID_INDEX || routes_[route_index].removed) {
        return nullptr;
    }

    return &routes_[route_index];
}


const RouteEntry* RoutingTable::lookup(uint32_t ip) const {
    uint32_t host_order_ip = ntohl(ip);
    uint32_t node_index = 0;

    uint32_t best_route_index = INVALID_INDEX;
    if (nodes_[node_index].route_index != INVALID_INDEX && !routes_[nodes_[node_index].route_index].removed) {
        best_route_index = nodes_[node_index].route_index;
    }

    for (uint8_t bit = 0; bit < 32; ++bit) {
        uint32_t direction = bit_at(host_order_ip, bit);
        uint32_t next = nodes_[node_index].child[direction];
        if (next == INVALID_INDEX) {
            break;
        }

        node_index = next;

        uint32_t route_index = nodes_[node_index].route_index;
        if (route_index != INVALID_INDEX && !routes_[route_index].removed) {
            best_route_index = route_index;
        }
    }

    if (best_route_index == INVALID_INDEX) {
        return nullptr;
    }

    return &routes_[best_route_index];
}

void RoutingTable::clear() {
    routes_.clear();
    nodes_.clear();
    nodes_.push_back({{INVALID_INDEX, INVALID_INDEX}, INVALID_INDEX});
}

size_t RoutingTable::node_count() const {
    return nodes_.size();
}

const std::deque<RouteEntry>& RoutingTable::routes() const {
    return routes_;
}

std::deque<RouteEntry>& RoutingTable::routes() {
    return routes_;
}