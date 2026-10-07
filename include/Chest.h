#pragma once
#include <REX/REX/Singleton.h>
#include <shared_mutex>
#include "API.h"

class EventSink;

namespace Serialization {
    void LoadCallback(SKSE::SerializationInterface* serializationInterface);
}

namespace UnownedStuff {
    constexpr RefID unownedChestOGRefID = 0x000EA29A;
    constexpr RefID unownedChestFormID = 0x000EA299;
    // RE::TESObjectCELL* unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000FE47B);  //
    // cwquartermastercontainers RE::TESObjectCONT* unownedChest =
    // RE::TESForm::LookupByID<RE::TESObjectCONT>(0x000A0DB5); // playerhousechestnew
    constexpr RE::NiPoint3 unownedChestPos = {1986.f, 1780.f, 6784.f};
    inline RE::TESObjectCELL* unownedCell = nullptr;
    inline RE::TESObjectCONT* unownedChest = nullptr;
}

class ChestManager final : public REX::Singleton<ChestManager> {
    friend class EventSink;
    friend void Serialization::LoadCallback(SKSE::SerializationInterface* serializationInterface);

    mutable std::shared_mutex register_mutex;

    std::unordered_map<RefID, ContainerizeAPI::ClientID> the_register;

    RE::TESObjectREFR* FindNotMatchedChest() const;
    [[nodiscard]] static bool IsEmpty(RE::TESObjectREFR* chest);
    [[nodiscard]] static RE::TESObjectREFR* MakeChest(RE::NiPoint3 Pos3 = {0.0f, 0.0f, 0.0f});
    static RE::TESObjectREFR* AddChest(uint32_t chest_no);

    void Reset();
    void RestoreContainerizeChests(const std::vector<RefID>& chest_refids);
    void HandleFormDelete(RefID chest_refid);

public:
    [[nodiscard]] bool Init();

    static uint32_t GetNoChests();
    static bool IsUnownedChest(RefID refid);
    [[nodiscard]] bool ReturnChest(ContainerizeAPI::ClientID client_id, RE::TESObjectREFR* chest);
    [[nodiscard]] RE::TESObjectREFR* RentChest(ContainerizeAPI::ClientID client_id);
};
