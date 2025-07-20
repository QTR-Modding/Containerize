#include "Serialization.h"

#include "Events.h"
#include "Manager.h"

bool Serialization::SaveLoadData::Save(SKSE::SerializationInterface* serializationInterface) {
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

bool Serialization::SaveLoadData::Save(SKSE::SerializationInterface* serializationInterface, std::uint32_t type,
    std::uint32_t version) {
    if (!serializationInterface->OpenRecord(type, version)) {
        logger::error("Failed to open record for Data Serialization!");
        return false;
    }

    return Save(serializationInterface);
}

bool Serialization::SaveLoadData::Load(SKSE::SerializationInterface* serializationInterface,
    const bool is_older_version) {
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

bool Serialization::DFSaveLoadData::Save(SKSE::SerializationInterface* serializationInterface) {
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

bool Serialization::DFSaveLoadData::Save(SKSE::SerializationInterface* serializationInterface, std::uint32_t type,
    std::uint32_t version) {
    if (!serializationInterface->OpenRecord(type, version)) {
        logger::error("Failed to open record for Data Serialization!");
        return false;
    }

    return Save(serializationInterface);
}

bool Serialization::DFSaveLoadData::Load(SKSE::SerializationInterface* serializationInterface, [[maybe_unused]] const bool cond) {
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
            logger::error("Failed to resolve form ID, 0x{:x}.", formid);
            continue;
        }

        std::string editorid;
        if (!read_string(serializationInterface, editorid)) {
            logger::error("Failed to read editorid");
            return false;
        }

        logger::trace("Formid:{:x}", formid);
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
                "rhs_ content: dyn_formid: {:x}, customid_bool: {},"
                "customid: {}, acteff_elapsed: {}",
                rhs_.dyn_formid, rhs_.custom_id.first, rhs_.custom_id.second, rhs_.acteff_elapsed);
            rhs.push_back(rhs_);
        }

        m_Data[lhs] = rhs;
        logger::info("Loaded data for formid {:x}, editorid {}", formid, editorid);
    }

    return true;
}

#define DISABLE_IF_UNINSTALLED if (Manager::GetSingleton()->isUninstalled) return;

void Serialization::SaveCallback(SKSE::SerializationInterface* serializationInterface) {
    DISABLE_IF_UNINSTALLED 
        logger::trace("Saving Data to skse co-save.");
    const auto M = Manager::GetSingleton();
    M->SendData();
    if (!M->Save(serializationInterface, Settings::kDataKey, Settings::kSerializationVersion)) {
        logger::critical("Failed to save Data");
    }
    auto* DFT = DynamicFormTracker::GetSingleton();
    DFT->SendData();
    if (!DFT->Save(serializationInterface, Settings::kDFDataKey, Settings::kSerializationVersion)) {
        logger::critical("Failed to save Data");
    }
    logger::trace("Data saved to skse co-save.");
}

void Serialization::LoadCallback(SKSE::SerializationInterface* serializationInterface) {
    DISABLE_IF_UNINSTALLED
    
    logger::info("Loading Data from skse co-save.");
    
    EventSink::GetSingleton()->Reset();
    Manager::GetSingleton()->Reset();
    auto* DFT = DynamicFormTracker::GetSingleton();
    DFT->Reset();

    std::uint32_t type;
    std::uint32_t version;
    std::uint32_t length;

    while (serializationInterface->GetNextRecordInfo(type, version, length)) {
        bool is_before_0_7 = false;
        
        auto temp = DecodeTypeCode(type);

        if (version == Settings::kSerializationVersion-3) {
            logger::warn("Loading data is from an older version < v0.7. Received ({}) - Expected ({}) for Data Key ({})",
                         version, Settings::kSerializationVersion, temp);

            is_before_0_7 = true;
            Settings::is_pre_0_7_1 = true;
            Settings::is_pre_0_10_0 = true;

            std::string err_message =
                "It seems you haven't followed the latest update instructions for the mod correctly. "
                "Please refer to the mod page for the latest instructions. "
                "In case of a failure you will see an error message box displayed after this one. If not, you are probably fine.";
            MsgBoxesNotifs::InGame::CustomMsg(err_message);
        }
        else if (version == Settings::kSerializationVersion - 2) {
            logger::warn("Loading data is from an older version < v0.7.1. Received ({}) - Expected ({}) for Data Key ({})",
                         version, Settings::kSerializationVersion, temp);

            Settings::is_pre_0_7_1 = true;
            Settings::is_pre_0_10_0 = true;
        }
        else if (version == Settings::kSerializationVersion - 1) {
            logger::warn("Loading data is from an older version < v0.10.0 Received ({}) - Expected ({}) for Data Key ({})",
                         version, Settings::kSerializationVersion, temp);

            Settings::is_pre_0_10_0 = true;
        }
        else if (version != Settings::kSerializationVersion) {
            logger::critical("Loaded data has incorrect version. Received ({}) - Expected ({}) for Data Key ({})",
                             version, Settings::kSerializationVersion, temp);
            continue;
        }
        switch (type) {
            case Settings::kDataKey: {
                logger::trace("Loading Record: {} - Version: {} - Length: {}", temp, version, length);
                if (!Manager::GetSingleton()->Load(serializationInterface, is_before_0_7)) {
                    logger::critical("Failed to Load Data");
                    return MsgBoxesNotifs::InGame::CustomMsg("Failed to Load Data.");
                }
            } break;
            case Settings::kDFDataKey: {
                logger::trace("Loading Record: {} - Version: {} - Length: {}", temp, version, length);
                if (!DFT->Load(serializationInterface, is_before_0_7)) logger::critical("Failed to Load Data for DFT");
            } break;
            default:
                logger::critical("Unrecognized Record Type: {}", temp);
                break;
        }
    }

    logger::info("Receiving Data.");
    DFT->ReceiveData();
    SKSE::GetTaskInterface()->AddTask([]() { 
            Manager::GetSingleton()->ReceiveData(); 
            logger::info("Data loaded from skse co-save.");
        }
        );
}

void Serialization::InitializeSerialization() {
    auto* serialization = SKSE::GetSerializationInterface();
    serialization->SetUniqueID(Settings::kDataKey);
    serialization->SetSaveCallback(SaveCallback);
    serialization->SetLoadCallback(LoadCallback);
    SKSE::log::trace("Cosave serialization initialized.");
}

#undef DISABLE_IF_UNINSTALLED