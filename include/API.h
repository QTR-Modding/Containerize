#pragma once

#include <cstdint>
#include <RE/B/BSCoreTypes.h>
#include <Windows.h>

namespace ContainerizeAPI {
    using ClientID = std::uint32_t;
    constexpr ClientID containerize_client = 0;
    constexpr ClientID invalid_client = 0;
    constexpr std::uint32_t interface_version = 1;

    struct Interface {
        ClientID (*RegisterClient)(const char* name);
        RE::FormID (*RentChest)(ClientID client);
        bool (*ReturnChest)(ClientID client, RE::FormID chest);
        bool (*Uninstall)(ClientID client);
        bool (*Refresh)(RE::FormID chest);
        RE::FormID (*GetChest)(RE::FormID target);
    };

    [[nodiscard]] inline const Interface* RequestAPI() {
        const auto module = GetModuleHandleW(L"Containerize.dll");
        if (!module) return nullptr;
        using Request = const Interface* (*)(std::uint32_t);
        const auto request = reinterpret_cast<Request>(GetProcAddress(module, "Containerize_RequestAPI"));
        return request ? request(interface_version) : nullptr;
    }
}
