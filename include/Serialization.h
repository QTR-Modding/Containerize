#pragma once
#include "Settings.h"
#include "CLibUtilsQTR/Serialization.hpp"

namespace Serialization {
    struct SaveDataRHS2 {
        FormID id; // fake formid
        bool equipped; // is equipped
        bool favorited; // is favorited
        RefID refid; // refid of unowned/realoutintheworld/externalcont

        SaveDataRHS2() : id(0), equipped(false), favorited(false), refid(0) {
        }
    };

    using SaveDataLHS = FormRefID;
    using SaveDataRHS = FormRefIDX;

    struct DFSaveData {
        FormID dyn_formid = 0;
        std::pair<bool, uint32_t> custom_id = {false, 0};
        float acteff_elapsed = -1.f;
    };

    using DFSaveDataLHS = std::pair<FormID, std::string>;
    using DFSaveDataRHS = std::vector<DFSaveData>;

    class SaveLoadData : public BaseData<SaveDataLHS, SaveDataRHS> {
    protected:
        ~SaveLoadData() = default;

    public:
        [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface) override;

        [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface, std::uint32_t type,
                                std::uint32_t version) override;

        [[nodiscard]] bool Load(SKSE::SerializationInterface* serializationInterface, bool is_older_version);
    };


    class DFSaveLoadData : public BaseData<DFSaveDataLHS, DFSaveDataRHS> {
    protected:
        ~DFSaveLoadData() = default;

    public:
        [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface) override;

        [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface, std::uint32_t type,
                                std::uint32_t version) override;

        [[nodiscard]] bool Load(SKSE::SerializationInterface* serializationInterface, bool);
    };

    void SaveCallback(SKSE::SerializationInterface* serializationInterface);

    void LoadCallback(SKSE::SerializationInterface* serializationInterface);

    void InitializeSerialization();
}