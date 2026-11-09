#pragma once

#include <cstdint>
#include <span>
#include <RE/B/BSCoreTypes.h>
#include <Windows.h>

namespace RE { class ExtraDataList; }

namespace ContainerizeAPI {
    using ClientID = std::uint32_t;
    constexpr ClientID containerize_client = 0;
    constexpr ClientID invalid_client = 0;
    constexpr std::uint32_t interface_version = 1;

    struct Interface {
        // ChestManager: rent and return storage chests; no item or furniture links.
        ClientID (*RegisterClient)(const char* name);
        RE::FormID (*RentChest)(ClientID client);
        bool (*ReturnChest)(ClientID client, RE::FormID chest);
        bool (*Uninstall)(ClientID client);

        // Containerize: storage linked to inventory items or placed objects.
        // A linked form is a fake inventory item or a placed reference.
        // A fake item is the dynamic copy Containerize displays instead of the original.
        bool (*Refresh)(RE::FormID chest);
        RE::FormID (*GetChest)(RE::FormID linkedForm);
        // Initialize also accepts an original item base FormID with its owning inventory.
        RE::FormID (*Initialize)(RE::FormID linkedForm, RE::FormID owner, RE::ExtraDataList* instance);
        bool (*Rename)(RE::FormID linkedForm, const char* name);
        bool (*DeRegister)(RE::FormID linkedForm);
        RE::FormID (*GetLinkedForm)(RE::FormID chest);
        RE::FormID (*GetRealID)(RE::FormID chest);
        // Returns the total matching count; fills as many chest RefIDs as the buffer holds.
        std::size_t (*GetChests)(RE::FormID baseForm, std::span<RE::FormID> chests);
    };

    [[nodiscard]] inline const Interface* RequestAPI() {
        const auto module = GetModuleHandleW(L"Containerize.dll");
        if (!module) return nullptr;
        using Request = const Interface* (*)(std::uint32_t);
        const auto request = reinterpret_cast<Request>(GetProcAddress(module, "Containerize_RequestAPI"));
        return request ? request(interface_version) : nullptr;
    }
}
