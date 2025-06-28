#pragma once
#include "Settings.h"


struct SaveDataRHS2 {
    FormID id;       // fake formid
    bool equipped;   // is equipped
    bool favorited;  // is favorited
    RefID refid;     // refid of unowned/realoutintheworld/externalcont

    SaveDataRHS2() : id(0), equipped(false), favorited(false), refid(0) {}
};

using SaveDataLHS = FormRefID;
using SaveDataRHS = FormRefIDX;

struct DFSaveData {
    FormID dyn_formid = 0;
    std::pair<bool, uint32_t> custom_id = {false, 0};
    float acteff_elapsed = -1.f;
};

using DFSaveDataLHS = std::pair<FormID,std::string>;
using DFSaveDataRHS = std::vector<DFSaveData>;

std::vector<std::pair<int, bool>> encodeString(const std::string& inputString);
std::string decodeString(const std::vector<std::pair<int, bool>>& encodedValues);
bool read_string(SKSE::SerializationInterface* a_intfc, std::string& a_str);
bool write_string(SKSE::SerializationInterface* a_intfc, const std::string& a_str);

// https :  // github.com/ozooma10/OSLAroused/blob/29ac62f220fadc63c829f6933e04be429d4f96b0/src/PersistedData.cpp
// BaseData is based off how powerof3's did it in Afterlife
template <typename T,typename U>
class BaseData {
public:
    float GetData(T formId, T missing) {
        Locker locker(m_Lock);
        // if the plugin version is less than 0.7 need to handle differently
        // if (SKSE::PluginInfo::version)
        if (auto idx = m_Data.contains(formId)) {
            return m_Data[formId];
        }
        return missing;
    }

    void SetData(T formId, U value) {
        Locker locker(m_Lock);
        m_Data[formId] = value;
    }

    virtual const char* GetType() = 0;

    virtual bool Save(SKSE::SerializationInterface*, std::uint32_t,
                      std::uint32_t) {return false;}
    virtual bool Save(SKSE::SerializationInterface*) {return false;}
    virtual bool Load(SKSE::SerializationInterface*, const bool) {return false;}

    void Clear() {
        Locker locker(m_Lock);
        m_Data.clear();
    }

protected:
    std::map<T,U> m_Data;

    using Lock = std::recursive_mutex;
    using Locker = std::lock_guard<Lock>;
    mutable Lock m_Lock;
};


class SaveLoadData : public BaseData<SaveDataLHS,SaveDataRHS> {
public:

    [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface) override {
        assert(serializationInterface);
        Locker locker(m_Lock);

        const auto numRecords = m_Data.size();
        if (!serializationInterface->WriteRecordData(numRecords)) {
            logger::error("Failed to save {} data records", numRecords);
            return false;
        }

        for (const auto& [formId, value] : m_Data) {
            if (!serializationInterface->WriteRecordData(formId)) {
                logger::error("Failed to save data for FormRefID: ({},{})", formId.outerKey, formId.innerKey);
                return false;
            }

            SaveDataRHS2 saveDataRHS;
            saveDataRHS.id = value.outerKey.id;
            saveDataRHS.equipped = value.outerKey.equipped;
            saveDataRHS.favorited = value.outerKey.favorited;
            saveDataRHS.refid = value.innerKey;

            if (!serializationInterface->WriteRecordData(saveDataRHS)) {
                logger::error("Failed to save value data for FormRefID: ({},{})", formId.outerKey, formId.innerKey);
                return false;
            }

            if (!write_string(serializationInterface, value.outerKey.name)) {
				logger::error("Failed to save name data for FormRefID: ({},{})", formId.outerKey, formId.innerKey);
				return false;
			}
        }
        return true;
    }

    [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface, std::uint32_t type,
                       std::uint32_t version) override {
        if (!serializationInterface->OpenRecord(type, version)) {
            logger::error("Failed to open record for Data Serialization!");
            return false;
        }

        return Save(serializationInterface);
    }

    [[nodiscard]] bool Load(SKSE::SerializationInterface* serializationInterface, const bool is_older_version) override {
        assert(serializationInterface);

        std::size_t recordDataSize;
        serializationInterface->ReadRecordData(recordDataSize);
        logger::trace("Loading data from serialization interface with size: {}", recordDataSize);

        Locker locker(m_Lock);
        m_Data.clear();


        for (size_t i = 0; i < recordDataSize; i++) {
            SaveDataLHS formId;
            SaveDataRHS value;
            logger::trace("Loading data from serialization interface.");
            logger::trace("FormID: ({},{}) serializationInterface->ReadRecordData:{}", formId.outerKey, formId.innerKey,
                        serializationInterface->ReadRecordData(formId));

            if (!serializationInterface->ResolveFormID(formId.outerKey, formId.outerKey)) {
                logger::error("Failed to resolve form ID, 0x{:X}.", formId.outerKey);
                continue;
            }
            
            if (is_older_version && !serializationInterface->ReadRecordData(value)) {
                    logger::error("Failed to load value data for FormRefID: ({},{})", formId.outerKey,
                                  formId.innerKey);
                    return false;
            } 
            else {
                SaveDataRHS2 saveDataRHS;
                logger::trace("Reading value...");
                if (!serializationInterface->ReadRecordData(saveDataRHS)) {
					logger::error("Failed to load value data for FormRefID: ({},{})", formId.outerKey, formId.innerKey);
					return false;
				}

                value.outerKey.id = saveDataRHS.id;
                value.outerKey.equipped = saveDataRHS.equipped;
                value.outerKey.favorited = saveDataRHS.favorited;
                value.innerKey = saveDataRHS.refid;

                if (!read_string(serializationInterface, value.outerKey.name)) {
                    	logger::error("Failed to load name data for FormRefID: ({},{})", formId.outerKey, formId.innerKey);
                }
            }

            m_Data[formId] = value;
            logger::trace("Loaded data for FormRefID: ({},{})", formId.outerKey, formId.innerKey);
        }
        return true;
    }
};



class DFSaveLoadData : public BaseData<DFSaveDataLHS, DFSaveDataRHS> {
public:

    [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface) override {
        assert(serializationInterface);
        Locker locker(m_Lock);

        const auto numRecords = m_Data.size();
        if (!serializationInterface->WriteRecordData(numRecords)) {
            logger::error("Failed to save {} data records", numRecords);
            return false;
        }

        for (const auto& [lhs, rhs] : m_Data) {
            // we serialize formid, editorid, and refid separately
            std::uint32_t formid = lhs.first;
            logger::trace("Formid:{}", formid);
            if (!serializationInterface->WriteRecordData(formid)) {
                logger::error("Failed to save formid");
                return false;
            }

            const std::string editorid = lhs.second;
            logger::trace("Editorid:{}", editorid);
            write_string(serializationInterface, editorid);

            // save the number of rhs records
            const auto numRhsRecords = rhs.size();
            if (!serializationInterface->WriteRecordData(numRhsRecords)) {
                logger::error("Failed to save the size {} of rhs records", numRhsRecords);
                return false;
            }

            for (const auto& rhs_ : rhs) {
                logger::trace("size of rhs_: {}", sizeof(rhs_));
                if (!serializationInterface->WriteRecordData(rhs_)) {
                    logger::error("Failed to save data");
                    return false;
                }
            }
        }
        return true;
    }

    [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface, std::uint32_t type,
                            std::uint32_t version) override {
        if (!serializationInterface->OpenRecord(type, version)) {
            logger::error("Failed to open record for Data Serialization!");
            return false;
        }

        return Save(serializationInterface);
    }

    [[nodiscard]] bool Load(SKSE::SerializationInterface* serializationInterface, const bool) override {
        assert(serializationInterface);

        std::size_t recordDataSize;
        serializationInterface->ReadRecordData(recordDataSize);
        logger::info("Loading data from serialization interface with size: {}", recordDataSize);

        Locker locker(m_Lock);
        m_Data.clear();

        logger::trace("Loading data from serialization interface.");
        for (size_t i = 0; i < recordDataSize; i++) {
            DFSaveDataRHS rhs;

            std::uint32_t formid = 0;
            logger::trace("ReadRecordData:{}", serializationInterface->ReadRecordData(formid));
            if (!serializationInterface->ResolveFormID(formid, formid)) {
                logger::error("Failed to resolve form ID, 0x{:X}.", formid);
                continue;
            }

            std::string editorid;
            if (!read_string(serializationInterface, editorid)) {
                logger::error("Failed to read editorid");
                return false;
            }

            logger::trace("Formid:{}", formid);
            logger::trace("Editorid:{}", editorid);

            DFSaveDataLHS lhs({formid, editorid});
            logger::trace("Reading value...");

            std::size_t rhsSize = 0;
            logger::trace("ReadRecordData: {}", serializationInterface->ReadRecordData(rhsSize));
            logger::trace("rhsSize: {}", rhsSize);

            for (size_t j = 0; j < rhsSize; j++) {
                DFSaveData rhs_;
                logger::trace("ReadRecordData: {}", serializationInterface->ReadRecordData(rhs_));
                logger::trace(
                    "rhs_ content: dyn_formid: {}, customid_bool: {},"
                    "customid: {}, acteff_elapsed: {}",
                    rhs_.dyn_formid, rhs_.custom_id.first, rhs_.custom_id.second, rhs_.acteff_elapsed);
                rhs.push_back(rhs_);
            }

            m_Data[lhs] = rhs;
            logger::info("Loaded data for formid {}, editorid {}", formid, editorid);
        }

        return true;
    }
};