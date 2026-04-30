//Oskar Kalinowski 352818
#pragma once

#include "common.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

constexpr uint32_t DISTANCE_INFINITY = 0xFFFFFFFFu;

class RoutingTable {
public:
	RoutingTable();

	// dodaje albo nadpisuje trase dla prefiksu.
	void insert(uint32_t network, uint8_t mask, const RouteEntry& data);

	// lpm lookup dla ip w kolejnosci sieciowej.
	// zwraca nullptr gdy brak pasujacej trasy.
	RouteEntry* lookup(uint32_t ip);

	// exact lookup dla pary network/maska.
	RouteEntry* lookup_exact(uint32_t network, uint8_t mask) ;

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
