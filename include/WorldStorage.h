#pragma once
#include <REX/REX/Singleton.h>
#include <shared_mutex>
#include "CustomObjects.h"

class WorldStorage final : public REX::Singleton<WorldStorage> {
    static constexpr auto client_name = "CTRZ_WS";

    mutable std::shared_mutex mutex_;
    std::unordered_map<RefID, RefID> chests;
    std::vector<Source> sources;

public:
    static constexpr std::array form_types{RE::TESFurniture::FORMTYPE};

    static constexpr std::uint32_t kDataKey = 'WSTZ';
    static constexpr std::uint32_t kSerializationVersion = 1;

    [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface);
    [[nodiscard]] bool Load(SKSE::SerializationInterface* serializationInterface, std::uint32_t length);
    void Reset();

    void LoadSources(std::vector<Source> configured_sources);
    [[nodiscard]] const Source* GetSource(FormID formid) const;
    [[nodiscard]] RefID GetTarget(RefID chest) const;
    void PlayCloseAnimation(RE::TESObjectREFR* chest);

    [[nodiscard]] RefID Initialize(RE::TESObjectREFR* target, RE::ExtraDataList* instance);
    [[nodiscard]] RefID Register(RE::TESObjectREFR* target);
    [[nodiscard]] RefID GetChest(RefID target) const;
    [[nodiscard]] bool DeRegister(RefID target);
};
