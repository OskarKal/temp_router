#pragma once

#include "common.h"

#include <chrono>
#include <netinet/in.h>
#include <vector>

// Sends all routing packets to all neighbors once.
// Returns true if every sendto call succeeded.
bool send_distances_once(
	int sockfd,
	const std::vector<RoutingPacket>& packets,
	const std::vector<sockaddr_in>& neighbors);
