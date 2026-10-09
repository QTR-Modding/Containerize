#pragma once
#include <REX/REX/Singleton.h>
#include <shared_mutex>
#include <unordered_set>
#include "API.h"

class EventSink;

namespace Serialization {
    void SaveCallback(SKSE::SerializationInterface* serializationInterface);
    void LoadCallback(SKSE::SerializationInterface* serializationInterface);
    void RevertCallback(SKSE::SerializationInterface* serializationInterface);
}

namespace UnownedStuff {
    // unowned stuff
    constexpr RefID unownedChestOGRefID = 0x000EA29A;
    constexpr RefID unownedChestFormID = 0x000EA299;
    //RE::TESObjectCELL* unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000FE47B);  // cwquartermastercontainers
    //RE::TESObjectCONT* unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(0x000A0DB5); // playerhousechestnew
    constexpr RE::NiPoint3 unownedChestPos = {1986.f, 1780.f, 6784.f};
    inline RE::TESObjectCELL* unownedCell = nullptr;
    inline RE::TESObjectCONT* unownedChest = nullptr;
}

class ChestManager final : public REX::Singleton<ChestManager> {
    friend class EventSink;
    friend void Serialization::SaveCallback(SKSE::SerializationInterface* serializationInterface);
    friend void Serialization::LoadCallback(SKSE::SerializationInterface* serializationInterface);
    friend void Serialization::RevertCallback(SKSE::SerializationInterface* serializationInterface);

    static constexpr std::uint32_t kDataKey = 'CHTZ';
    static constexpr std::uint32_t kSerializationVersion = 1;

    struct RentalState {
        std::unordered_map<RefID, ContainerizeAPI::ClientID> rented;
        std::unordered_set<RefID> returned;
    };

    mutable std::shared_mutex register_mutex;

    RentalState rental_state;

    RE::TESObjectREFR* FindNotMatchedChest() const;
    [[nodiscard]] static RE::TESObjectREFR* MakeChest(RE::NiPoint3 Pos3 = {0.0f, 0.0f, 0.0f});
    static RE::TESObjectREFR* AddChest(uint32_t chest_no);

    [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface);
    [[nodiscard]] bool Load(SKSE::SerializationInterface* serializationInterface, std::uint32_t length);
    void Reset();
    void ResumeDisposals();
    void ScheduleDisposal(RE::TESObjectREFR* chest);
    void DisposeReturnedChest(RE::ObjectRefHandle chest_handle);
    void RestoreContainerizeChests(const std::vector<RefID>& chest_refids);
    void HandleFormDelete(RefID chest_refid);

public:
    [[nodiscard]] bool Init();
    [[nodiscard]] static bool IsEmpty(RE::TESObjectREFR* chest);

    static uint32_t GetNoChests();
    static bool IsUnownedChest(RefID refid);
    [[nodiscard]] bool ReturnChest(ContainerizeAPI::ClientID client_id, RE::TESObjectREFR* chest);
    [[nodiscard]] RE::TESObjectREFR* RentChest(ContainerizeAPI::ClientID client_id);
};
