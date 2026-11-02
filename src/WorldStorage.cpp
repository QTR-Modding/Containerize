#include "WorldStorage.h"
#include "Chest.h"
#include "Utils.h"

RefID WorldStorage::Register(RE::TESObjectREFR* target) {
    const auto target_id = target->GetFormID();
    if (const auto chest_id = GetChest(target_id)) return chest_id;

    const auto chest_manager = ChestManager::GetSingleton();
    const auto client_id = chest_manager->RegisterClient(client_name);
    if (client_id == ContainerizeAPI::invalid_client) return 0;
    const auto chest = chest_manager->RentChest(client_id);
    if (!chest) return 0;

    const auto chest_id = chest->GetFormID();
    RefID registered_chest;
    {
        std::unique_lock lock(mutex_);
        const auto [it, inserted] = chests.emplace(target_id, chest_id);
        if (inserted) return chest_id;
        registered_chest = it->second;
    }
    if (!chest_manager->ReturnChest(client_id, chest)) {
        logger::error("Failed to return unused chest {:x} for client {}", chest_id, client_id);
    }
    return registered_chest;
}

RefID WorldStorage::GetChest(const RefID target) const {
    std::shared_lock lock(mutex_);
    const auto it = chests.find(target);
    return it != chests.end() ? it->second : 0;
}

bool WorldStorage::DeRegister(const RefID target) {
    RE::TESObjectREFR* chest;
    RefID chest_id;
    {
        std::unique_lock lock(mutex_);
        const auto it = chests.find(target);
        if (it == chests.end()) return false;
        chest_id = it->second;
        chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_id);
        if (!chest) return false;
        chests.erase(it);
    }

    const auto player = RE::PlayerCharacter::GetSingleton();
    for (auto& [item, entry] : chest->GetInventory()) {
        if (entry.first <= 0) continue;
        chest->RemoveItem(item, entry.first, RE::ITEM_REMOVE_REASON::kRemove, nullptr, player);
    }
    if (!ChestManager::IsEmpty(chest)) MsgBoxesNotifs::InGame::ReportTransferFailure(chest_id);
    const auto chest_manager = ChestManager::GetSingleton();
    const auto client_id = chest_manager->RegisterClient(client_name);
    if (!chest_manager->ReturnChest(client_id, chest)) {
        logger::critical("Failed to return chest {:x} for client {}", chest_id, client_id);
        return false;
    }
    if (const auto changes = player->GetInventoryChanges(true)) changes->GetInventoryWeight();
    return true;
}

bool WorldStorage::Save(SKSE::SerializationInterface* serializationInterface) {
    std::shared_lock lock(mutex_);
    if (!serializationInterface->OpenRecord(kDataKey, kSerializationVersion)) return false;
    for (const auto& [target, chest] : chests) {
        if (!serializationInterface->WriteRecordData(target) ||
            !serializationInterface->WriteRecordData(chest)) return false;
    }
    return true;
}

bool WorldStorage::Load(SKSE::SerializationInterface* serializationInterface, const std::uint32_t length) {
    constexpr auto entry_size = sizeof(RefID) + sizeof(RefID);
    if (length % entry_size != 0) return false;
    std::unordered_map<RefID, RefID> loaded;
    for (std::uint32_t i = 0; i < length / entry_size; ++i) {
        RefID target;
        RefID chest;
        if (serializationInterface->ReadRecordData(target) != sizeof(target) ||
            serializationInterface->ReadRecordData(chest) != sizeof(chest)) return false;
        if (!serializationInterface->ResolveFormID(target, target) ||
            !serializationInterface->ResolveFormID(chest, chest)) {
            logger::warn("Could not resolve world storage {:x}, chest {:x} during load", target, chest);
            continue;
        }
        if (!loaded.emplace(target, chest).second) return false;
    }
    std::unique_lock lock(mutex_);
    chests = std::move(loaded);
    return true;
}

void WorldStorage::Reset() {
    std::unique_lock lock(mutex_);
    chests.clear();
}
