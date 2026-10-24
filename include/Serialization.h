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

    class SaveLoadData : public BaseData<SaveDataLHS, SaveDataRHS> {
    protected:
        ~SaveLoadData() = default;

    public:
        [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface) override;

        [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface, std::uint32_t type,
                                std::uint32_t version) override;

        [[nodiscard]] bool Load(SKSE::SerializationInterface* serializationInterface, bool is_older_version);

        [[nodiscard]] std::vector<RefID> GetLoadedChestRefIDs();
    };


    void SaveCallback(SKSE::SerializationInterface* serializationInterface);

    void LoadCallback(SKSE::SerializationInterface* serializationInterface);

    void RevertCallback(SKSE::SerializationInterface* serializationInterface);

    void InitializeSerialization();
}