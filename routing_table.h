#pragma once

#include "common.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

// Infinity distance value used by routing protocol.
constexpr uint32_t DISTANCE_INFINITY = 0xFFFFFFFFu;

class RoutingTable {
public:
	RoutingTable();

	// Inserts or updates route for a CIDR prefix.
	// network must be in network byte order, mask in range [0, 32].
	void insert(uint32_t network, uint8_t mask, const RouteEntry& data);

	// Longest Prefix Match lookup for destination IP in network byte order.
	// Returns nullptr when no route is available.
	RouteEntry* lookup(uint32_t ip);
	const RouteEntry* lookup(uint32_t ip) const;

	// Exact prefix lookup for network/mask pair in network byte order.
	RouteEntry* lookup_exact(uint32_t network, uint8_t mask);
	const RouteEntry* lookup_exact(uint32_t network, uint8_t mask) const;

	void clear();
	size_t node_count() const;

	const std::deque<RouteEntry>& routes() const;
	std::deque<RouteEntry>& routes();

private:
	static constexpr uint32_t INVALID_INDEX = 0xFFFFFFFFu;

	struct Node {
		uint32_t child[2];
		uint32_t route_index;
	};

	std::vector<Node> nodes_;
	std::deque<RouteEntry> routes_;

	static inline uint32_t bit_at(uint32_t host_order_ip, uint8_t bit_index) {
		return (host_order_ip >> (31u - bit_index)) & 1u;
	}
};
