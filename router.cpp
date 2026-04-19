#include "common.h"
#include "config.h"
#include "distance_vector.h"
#include "routing_table.h"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {
constexpr uint16_t kRouterPort = 54321;
constexpr auto kTurnInterval = std::chrono::seconds(5);
constexpr uint64_t kNeighborTimeoutTurns = 2;
constexpr uint64_t kStaleRemovalTurns = 4;
constexpr uint64_t kDirectStopAdvertisingTurns = 3;

uint32_t host_mask(uint8_t mask) {
    if (mask == 0) {
        return 0u;
    }

    if (mask == 32) {
        return 0xFFFFFFFFu;
    }

    return 0xFFFFFFFFu << (32u - mask);
}

uint32_t network_prefix(uint32_t ip_net, uint8_t mask) {
    uint32_t host_ip = ntohl(ip_net);
    uint32_t mask_bits = host_mask(mask);
    return htonl(host_ip & mask_bits);
}

uint32_t broadcast_address(uint32_t ip_net, uint8_t mask) {
    uint32_t host_ip = ntohl(ip_net);
    uint32_t mask_bits = host_mask(mask);
    return htonl((host_ip & mask_bits) | ~mask_bits);
}

bool should_advertise(const RouteEntry& route, uint64_t turn) {
    if (route.removed) {
        return false;
    }

    if (route.is_direct) {
        if (route.distance != DISTANCE_INFINITY) {
            return true;
        }

        return turn - route.last_seen < kDirectStopAdvertisingTurns;
    }

    if (route.distance != DISTANCE_INFINITY) {
        return true;
    }

    return turn - route.last_seen < kStaleRemovalTurns;
}

void initialize_direct_routes(RoutingTable& table, const std::vector<InterfaceInfo>& interfaces) {
    for (const InterfaceInfo& iface : interfaces) {
        RouteEntry entry{};
        entry.network = network_prefix(iface.ip_net, iface.mask);
        entry.mask = iface.mask;
        entry.distance = ntohl(iface.distance);
        entry.via_ip = 0;
        entry.last_seen = 0;
        entry.direct_distance = entry.distance;
        entry.is_direct = true;
        entry.removed = false;
        table.insert(entry.network, entry.mask, entry);
    }
}

void print_route(const RouteEntry& route) {
    in_addr network_addr{};
    network_addr.s_addr = route.network;

    char network_buffer[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &network_addr, network_buffer, sizeof(network_buffer)) == nullptr) {
        return;
    }

    if (route.is_direct) {
        if (route.distance == DISTANCE_INFINITY) {
            std::printf("%s/%u unreachable connected directly\n", network_buffer, route.mask);
        } else {
            std::printf("%s/%u distance %u connected directly\n", network_buffer, route.mask, route.distance);
        }
        return;
    }

    in_addr via_addr{};
    via_addr.s_addr = route.via_ip;

    char via_buffer[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &via_addr, via_buffer, sizeof(via_buffer)) == nullptr) {
        return;
    }

    if (route.distance == DISTANCE_INFINITY) {
        std::printf("%s/%u unreachable via %s\n", network_buffer, route.mask, via_buffer);
    } else {
        std::printf("%s/%u distance %u via %s\n", network_buffer, route.mask, route.distance, via_buffer);
    }
}

void print_routing_table(const RoutingTable& table) {
    for (const RouteEntry& route : table.routes()) {
        if (route.removed) {
            continue;
        }
        print_route(route);
    }
    std::fflush(stdout);
}

void age_routes(RoutingTable& table, uint64_t turn) {
    for (RouteEntry& route : table.routes()) {
        if (route.removed) {
            continue;
        }

        uint64_t age = turn - route.last_seen;

        if (route.is_direct) {
            if (route.distance != DISTANCE_INFINITY && age >= kNeighborTimeoutTurns) {
                route.distance = DISTANCE_INFINITY;
            }
            continue;
        }

        if (route.distance != DISTANCE_INFINITY && age >= kNeighborTimeoutTurns) {
            route.distance = DISTANCE_INFINITY;
        }

        if (route.distance == DISTANCE_INFINITY && age >= kStaleRemovalTurns) {
            route.removed = true;
        }
    }
}

bool send_routes_to_neighbor(
    int sockfd,
    RoutingTable& table,
    const sockaddr_in& neighbor,
    const InterfaceInfo& iface,
    uint64_t turn) {
    bool any_failure = false;
    bool any_success = false;

    for (const RouteEntry& route : table.routes()) {
        if (!should_advertise(route, turn)) {
            continue;
        }

        RoutingPacket packet{};
        packet.ip_net = route.network;
        packet.mask = route.mask;
        packet.distance = htonl(route.distance);

        ssize_t sent = sendto(
            sockfd,
            &packet,
            sizeof(packet),
            0,
            reinterpret_cast<const sockaddr*>(&neighbor),
            sizeof(neighbor));

        if (sent != static_cast<ssize_t>(sizeof(packet))) {
            any_failure = true;
        } else {
            any_success = true;
        }
    }

    uint32_t direct_network = network_prefix(iface.ip_net, iface.mask);
    RouteEntry* direct_route = table.lookup_exact(direct_network, iface.mask);
    if (direct_route != nullptr) {
        if (any_failure) {
            direct_route->distance = DISTANCE_INFINITY;
        } else if (any_success) {
            direct_route->distance = direct_route->direct_distance;
            direct_route->last_seen = turn;
        }
    }

    return !any_failure;
}

void send_current_vector(
    int sockfd,
    RoutingTable& table,
    const std::vector<InterfaceInfo>& interfaces,
    const std::vector<sockaddr_in>& neighbors,
    uint64_t turn) {
    for (std::size_t index = 0; index < neighbors.size(); ++index) {
        (void)send_routes_to_neighbor(sockfd, table, neighbors[index], interfaces[index], turn);
    }
}

void process_packet(RoutingTable& table, uint32_t sender_ip, uint64_t turn, const uint8_t* buffer, std::size_t size) {
    if (size != sizeof(RoutingPacket)) {
        return;
    }

    RoutingPacket packet{};
    std::memcpy(&packet, buffer, sizeof(packet));
    update_distance_vector(&table, sender_ip, turn, packet);
}

void receive_pending_packets(int sockfd, RoutingTable& table, uint64_t turn) {
    for (;;) {
        uint8_t buffer[sizeof(RoutingPacket)];
        sockaddr_in source{};
        socklen_t source_length = sizeof(source);

        ssize_t received = recvfrom(
            sockfd,
            buffer,
            sizeof(buffer),
            MSG_DONTWAIT,
            reinterpret_cast<sockaddr*>(&source),
            &source_length);

        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            if (errno == EINTR) {
                continue;
            }
            return;
        }

        uint32_t sender_ip = source.sin_addr.s_addr;
        process_packet(table, sender_ip, turn, buffer, static_cast<std::size_t>(received));
    }
}

void setup_socket(int sockfd) {
    int enabled = 1;
    (void)setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
    (void)setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &enabled, sizeof(enabled));
}

bool bind_socket(int sockfd) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(kRouterPort);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    return bind(sockfd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
}

}  // namespace

int main() {
    std::vector<InterfaceInfo> interfaces = getConfig();

    RoutingTable table;
    initialize_direct_routes(table, interfaces);

    std::vector<sockaddr_in> neighbors;
    neighbors.reserve(interfaces.size());
    for (const InterfaceInfo& iface : interfaces) {
        sockaddr_in neighbor{};
        neighbor.sin_family = AF_INET;
        neighbor.sin_port = htons(kRouterPort);
        neighbor.sin_addr.s_addr = broadcast_address(iface.ip_net, iface.mask);
        neighbors.push_back(neighbor);
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::perror("socket");
        return 1;
    }

    setup_socket(sockfd);
    if (!bind_socket(sockfd)) {
        std::perror("bind");
        close(sockfd);
        return 1;
    }

    using clock = std::chrono::steady_clock;
    auto next_turn = clock::now() + kTurnInterval;
    uint64_t turn = 0;

    while (true) {
        auto now = clock::now();
        if (now >= next_turn) {
            age_routes(table, turn);
            send_current_vector(sockfd, table, interfaces, neighbors, turn);
            print_routing_table(table);
            ++turn;
            next_turn += kTurnInterval;
            continue;
        }

        auto wait_duration = next_turn - now;
        timeval timeout{};
        timeout.tv_sec = static_cast<time_t>(std::chrono::duration_cast<std::chrono::seconds>(wait_duration).count());
        timeout.tv_usec = static_cast<suseconds_t>(std::chrono::duration_cast<std::chrono::microseconds>(wait_duration - std::chrono::seconds(timeout.tv_sec)).count());

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);

        int ready = select(sockfd + 1, &read_fds, nullptr, nullptr, &timeout);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::perror("select");
            break;
        }

        if (ready > 0 && FD_ISSET(sockfd, &read_fds)) {
            receive_pending_packets(sockfd, table, turn);
        }
    }

    close(sockfd);
    return 0;
}
