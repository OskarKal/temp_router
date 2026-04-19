#include "distance_vector.h"

#include "routing_table.h"

#include <arpa/inet.h>

namespace {
constexpr uint32_t kDangerousDistanceThreshold = 1u << 28;

uint32_t normalize_distance(uint32_t advertised_distance) {
    if (advertised_distance >= kDangerousDistanceThreshold) {
        return DISTANCE_INFINITY;
    }

    uint32_t next_distance = advertised_distance + 1u;
    if (next_distance >= kDangerousDistanceThreshold || next_distance < advertised_distance) {
        return DISTANCE_INFINITY;
    }

    return next_distance;
}
}  // namespace

void update_distance_vector(
    RoutingTable* table,
    uint32_t sender_ip,
    uint64_t turn,
    const RoutingPacket& packet) {
    if (table == nullptr) {
        return;
    }

    RouteEntry entry{};
    entry.network = packet.ip_net;
    entry.mask = packet.mask;
    entry.distance = normalize_distance(ntohl(packet.distance));
    entry.via_ip = sender_ip;
    entry.last_seen = turn;
    entry.direct_distance = 0;
    entry.is_direct = false;
    entry.removed = false;

    const RouteEntry* current = table->lookup_exact(packet.ip_net, packet.mask);
    if (current != nullptr) {
        if (current->is_direct) {
            return;
        }

        bool candidate_reachable = (entry.distance != DISTANCE_INFINITY);

        if (current->via_ip != sender_ip) {
            if (!candidate_reachable) {
                return;
            }

            if (current->distance != DISTANCE_INFINITY && entry.distance >= current->distance) {
                return;
            }
        }
    }

    table->insert(packet.ip_net, packet.mask, entry);
}
