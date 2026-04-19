#pragma once

#include "common.h"

class RoutingTable;

void update_distance_vector(
	RoutingTable* table,
	uint32_t sender_ip,
	uint64_t turn,
	const RoutingPacket& packet);
