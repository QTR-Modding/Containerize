#include "Chest.h"
#include "Settings.h"

bool ChestManager::Init() {
    using namespace UnownedStuff;
    const auto unownedChestOG = RE::TESForm::LookupByID<RE::TESObjectREFR>(0x000EA29A);
    unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(unownedChestFormID);
    unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000EA28B);
    if (!unownedChestOG || !unownedChestOG->GetBaseObject() || !unownedCell || !unownedChest ||
        unownedChestOG->GetBaseObject()->GetFormID() != unownedChest->GetFormID() ||
        !unownedChest->As<RE::TESBoundObject>()) {
        logger::error("Missing unowned chest/cell");
        return false;
    }

    if (Settings::is_pre_0_7_1 && unownedChestOG) {
        const auto player_ref = RE::PlayerCharacter::GetSingleton();
        if (!player_ref) return false;
        for (auto& [fst, snd] : unownedChestOG->GetInventory()) {
            unownedChestOG->RemoveItem(fst, snd.first, RE::ITEM_REMOVE_REASON::kRemove, nullptr, player_ref);
            if (fst->IsDynamicForm())
                player_ref->RemoveItem(fst, snd.first, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
        }
    }

    return true;
}

bool ChestManager::IsEmpty(RE::TESObjectREFR* chest) {
    return chest && chest->GetInventory().empty();
}

RE::TESObjectREFR* ChestManager::FindNotMatchedChest() const {
    auto& runtimeData = UnownedStuff::unownedCell->GetRuntimeData();
    RE::BSSpinLockGuard locker(runtimeData.spinLock);
    for (const auto& ref : runtimeData.references) {
        if (!ref) continue;
        if (ref->GetFormID() == UnownedStuff::unownedChestOGRefID) continue;
        if (ref->GetBaseObject()->GetFormID() != UnownedStuff::unownedChest->GetFormID()) continue;
        if (!the_register.contains(ref->GetFormID()) && IsEmpty(ref.get())) {
            return ref.get();
        }
    }
    return AddChest(GetNoChests());
}

uint32_t ChestManager::GetNoChests() {
    uint32_t no_chests = 0;
    auto& runtimeData = UnownedStuff::unownedCell->GetRuntimeData();
    RE::BSSpinLockGuard locker(runtimeData.spinLock);
    for (const auto& ref : runtimeData.references) {
        if (!ref) continue;
        if (ref->IsDeleted()) continue;
        if (ref->GetBaseObject()->GetFormID() == UnownedStuff::unownedChest->GetFormID()) {
            no_chests++;
        }
    }
    return no_chests;
}

RE::TESObjectREFR* ChestManager::MakeChest(RE::NiPoint3 Pos3) {
    const auto item = UnownedStuff::unownedChest->As<RE::TESBoundObject>();
    const auto newPropRef = RE::TESDataHandler::GetSingleton()
                            ->CreateReferenceAtLocation(item, Pos3, {0.0f, 0.0f, 0.0f}, UnownedStuff::unownedCell,
                                                        nullptr,
                                                        nullptr, nullptr, {}, true, false)
                            .get()
                            .get();
    logger::info("Created Object! Type: {}, Base ID: {:x}, Ref ID: {:x},",
                 RE::FormTypeToString(item->GetFormType()), item->GetFormID(), newPropRef->GetFormID());
    return newPropRef;
}

RE::TESObjectREFR* ChestManager::AddChest(const uint32_t chest_no) {
    int total_chests = static_cast<int>(chest_no);
    total_chests += 1;
    const int total_chests_x = (1 - (total_chests % 3)) * (-2);
    const int total_chests_y = ((total_chests - 1) / 3) % 9;
    const int total_chests_z = (total_chests - 1) / 27;
    const float Pos3_x = UnownedStuff::unownedChestPos.x + static_cast<float>(100 * total_chests_x);
    const float Pos3_y = UnownedStuff::unownedChestPos.y + static_cast<float>(50 * total_chests_y);
    const float Pos3_z = UnownedStuff::unownedChestPos.z + static_cast<float>(50 * total_chests_z);
    const RE::NiPoint3 Pos3 = {Pos3_x, Pos3_y, Pos3_z};
    return MakeChest(Pos3);
}

bool ChestManager::IsUnownedChest(const RefID refid) {
    const auto temp = RE::TESForm::LookupByID<RE::TESObjectREFR>(refid);
    if (!temp) return false;
    const auto base = temp->GetBaseObject();
    return base ? base->GetFormID() == UnownedStuff::unownedChest->GetFormID() : false;
}

void ChestManager::Reset() {
    std::unique_lock lock(register_mutex);
    the_register.clear();
}

void ChestManager::RestoreContainerizeChests(const std::vector<RefID>& chest_refids) {
    std::unique_lock lock(register_mutex);
    for (const auto chest_refid : chest_refids) {
        the_register.insert_or_assign(chest_refid, ContainerizeAPI::containerize_client);
    }
}

void ChestManager::HandleFormDelete(const RefID chest_refid) {
    std::unique_lock lock(register_mutex);
    the_register.erase(chest_refid);
}

bool ChestManager::ReturnChest(const ContainerizeAPI::ClientID client_id, RE::TESObjectREFR* chest) {
    if (!chest) return false;
    std::unique_lock lock(register_mutex);
    const auto lease = the_register.find(chest->GetFormID());
    if (lease == the_register.end() || lease->second != client_id) {
        return true;
    }
    if (!IsEmpty(chest)) {
        logger::error("Client {} tried to return nonempty chest {:x}", client_id, chest->GetFormID());
        return false;
    }

    the_register.erase(lease);
    return true;
}

RE::TESObjectREFR* ChestManager::RentChest(const ContainerizeAPI::ClientID client_id) {
    std::unique_lock lock(register_mutex);
    if (const auto a_chest = FindNotMatchedChest()) {
        if (the_register.emplace(a_chest->GetFormID(), client_id).second) {
            return a_chest;
        }
        logger::error("Failed to register chest for client {}. Chest ID: {:x}", client_id, a_chest->GetFormID());
        return nullptr;
    }
    return nullptr;
}
