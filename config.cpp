//Oskar Kalinowski 352818
#include "config.h"

#include <arpa/inet.h>

#include <cstdio>
#include <cstdint>
#include <vector>

namespace {
// bufor na linie z konfiguracja.
constexpr int kBufferSize = 128;
}

// parser wejscia: liczba interfejsow + linie ip/maska distance x.
std::vector<InterfaceInfo> getConfig() {
    char buffer[kBufferSize];
    if (std::fgets(buffer, kBufferSize, stdin) == nullptr) {
        return {};
    }

    int count = 0;
    if (std::sscanf(buffer, "%d", &count) != 1 || count < 0) {
        return {};
    }

    std::vector<InterfaceInfo> interfaces;
    interfaces.reserve(static_cast<std::size_t>(count));

    for (int i = 0; i < count; ++i) {
        if (std::fgets(buffer, kBufferSize, stdin) == nullptr) {
            break;
        }

        unsigned int a = 0;
        unsigned int b = 0;
        unsigned int c = 0;
        unsigned int d = 0;
        unsigned int mask = 0;
        unsigned int distance = 0;

        if (std::sscanf(
                buffer,
                "%u.%u.%u.%u/%u distance %u",
                &a,
                &b,
                &c,
                &d,
                &mask,
                &distance) != 6) {
            continue;
        }

        uint32_t ip_net = (a << 24u) | (b << 16u) | (c << 8u) | d;
        interfaces.push_back({htonl(ip_net), static_cast<uint8_t>(mask), htonl(distance)});
    }

    return interfaces;
}
