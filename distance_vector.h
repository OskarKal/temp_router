
//Oskar Kalinowski 352818
#pragma once

#include "common.h"

class RoutingTable;

// aktualizacja dv na podstawie jednego pakietu od nadawcy.
void update_distance_vector(
	RoutingTable* table,
	uint32_t sender_ip,
	uint64_t turn,
	const RoutingPacket& packet);
