//Oskar Kalinowski 352818

#include "distance_vector.h"

#include "routing_table.h"

#include <arpa/inet.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace {
// prog po ktorym dystans uznajemy za nieskonczonosc.
constexpr uint32_t kDangerousDistanceThreshold = 1u << 28;

bool debug_enabled() {
    const char* env = std::getenv("ROUTER_DEBUG");
    return env != nullptr && env[0] != '\0' && env[0] != '0';
}

void debug_log(const char* format, ...) {
    if (!debug_enabled()) {
        return;
    }

    std::va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    std::fprintf(stderr, "\n");
    va_end(args);
}

const char* ip_to_str(uint32_t ip_net_order, char* buffer, std::size_t size) {
    in_addr addr{};
    addr.s_addr = ip_net_order;
    if (inet_ntop(AF_INET, &addr, buffer, static_cast<socklen_t>(size)) == nullptr) {
        std::snprintf(buffer, size, "<invalid>");
    }
    return buffer;
}

// bezpieczne dodawanie kosztow z obcieciem do inf.
uint32_t add_distance_with_cap(uint32_t left, uint32_t right) {
    if (left >= kDangerousDistanceThreshold || right >= kDangerousDistanceThreshold) {
        return DISTANCE_INFINITY;
    }

    uint32_t total = left + right;
    if (total >= kDangerousDistanceThreshold || total < left || total < right) {
        return DISTANCE_INFINITY;
    }

    return total;
}
}  // namespace

// bellman-forda dla jednego wpisu.
void update_distance_vector(
    RoutingTable* table,
    uint32_t sender_ip,
    uint64_t turn,
    const RoutingPacket& packet) {
    if (table == nullptr) {
        debug_log("[DV] skip: table=null");
        return;
    }

    RouteEntry* direct_to_sender = table->lookup(sender_ip);
    if (direct_to_sender == nullptr || !direct_to_sender->is_direct) {
        char sender_buf[INET_ADDRSTRLEN];
        debug_log("[DV] skip: sender %s not reachable as direct neighbor", ip_to_str(sender_ip, sender_buf, sizeof(sender_buf)));
        return;
    }

    direct_to_sender->last_seen = turn;
    direct_to_sender->distance = direct_to_sender->direct_distance;

    RouteEntry entry{};
    entry.network = packet.ip_net;
    entry.mask = packet.mask;
    entry.distance = add_distance_with_cap(direct_to_sender->distance, ntohl(packet.distance));
    entry.via_ip = sender_ip;
    entry.last_seen = turn;
    entry.direct_distance = 0;
    entry.is_direct = false;
    entry.removed = false;

    {
        char sender_buf[INET_ADDRSTRLEN];
        char net_buf[INET_ADDRSTRLEN];
        debug_log(
            "[DV] recv: from=%s net=%s/%u adv=%u link=%u cand=%u turn=%llu",
            ip_to_str(sender_ip, sender_buf, sizeof(sender_buf)),
            ip_to_str(packet.ip_net, net_buf, sizeof(net_buf)),
            packet.mask,
            ntohl(packet.distance),
            direct_to_sender->distance,
            entry.distance,
            static_cast<unsigned long long>(turn));
    }

    const RouteEntry* current = table->lookup_exact(packet.ip_net, packet.mask);
    if (current != nullptr) {
        if (current->is_direct) {
            char net_buf[INET_ADDRSTRLEN];
            debug_log("[DV] keep direct: net=%s/%u", ip_to_str(packet.ip_net, net_buf, sizeof(net_buf)), packet.mask);
            return;
        }

        bool candidate_reachable = (entry.distance != DISTANCE_INFINITY);

        if (current->via_ip != sender_ip) {
            if (!candidate_reachable) {
                char net_buf[INET_ADDRSTRLEN];
                debug_log("[DV] reject: net=%s/%u candidate unreachable from other next-hop", ip_to_str(packet.ip_net, net_buf, sizeof(net_buf)), packet.mask);
                return;
            }

            if (current->distance != DISTANCE_INFINITY && entry.distance >= current->distance) {
                char net_buf[INET_ADDRSTRLEN];
                debug_log(
                    "[DV] reject: net=%s/%u cand=%u current=%u (not better)",
                    ip_to_str(packet.ip_net, net_buf, sizeof(net_buf)),
                    packet.mask,
                    entry.distance,
                    current->distance);
                return;
            }
        }
    }

    table->insert(packet.ip_net, packet.mask, entry);
    {
        char sender_buf[INET_ADDRSTRLEN];
        char net_buf[INET_ADDRSTRLEN];
        debug_log(
            "[DV] install: net=%s/%u dist=%u via=%s",
            ip_to_str(packet.ip_net, net_buf, sizeof(net_buf)),
            packet.mask,
            entry.distance,
            ip_to_str(sender_ip, sender_buf, sizeof(sender_buf)));
    }
}
