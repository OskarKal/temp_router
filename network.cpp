#include "network.h"

#include <sys/socket.h>

#include <chrono>
#include <thread>

bool send_distances_once(
	int sockfd,
	const std::vector<RoutingPacket>& packets,
	const std::vector<sockaddr_in>& neighbors) {
	bool all_ok = true;

	for (const sockaddr_in& neighbor : neighbors) {
		for (const RoutingPacket& packet : packets) {
			ssize_t sent = sendto(
				sockfd,
				&packet,
				sizeof(packet),
				0,
				reinterpret_cast<const sockaddr*>(&neighbor),
				sizeof(neighbor));

			if (sent != static_cast<ssize_t>(sizeof(packet))) {
				all_ok = false;
			}
		}
	}

	return all_ok;
}

