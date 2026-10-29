#include "Chest.h"

bool ChestManager::Init() {
    using namespace UnownedStuff;
    const auto unownedChestOG = RE::TESForm::LookupByID<RE::TESObjectREFR>(0x000EA29A);
    unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(unownedChestFormID);
    unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000EA28B);
    if (!unownedChestOG || !unownedChestOG->GetBaseObject() || !unownedCell || !unownedChest ||
        unownedChestOG->GetBaseObject()->GetFormID() != unownedChest->GetFormID() ||
        !unownedChest->As<RE::TESBoundObject>()) {
        unownedCell = nullptr;
        logger::error("Missing unowned chest/cell");
        return false;
    }

    return true;
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

bool ChestManager::IsUnownedChest(const RefID refid) {
    const auto temp = RE::TESForm::LookupByID<RE::TESObjectREFR>(refid);
    if (!temp) return false;
    const auto base = temp->GetBaseObject();
    return base ? base->GetFormID() == UnownedStuff::unownedChest->GetFormID() : false;
}

RE::TESObjectREFR* ChestManager::MakeChest(const RE::NiPoint3 Pos3) {
    const auto item = UnownedStuff::unownedChest->As<RE::TESBoundObject>();
    const auto newPropRef = RE::TESDataHandler::GetSingleton()
                            ->CreateReferenceAtLocation(item, Pos3, {0.0f, 0.0f, 0.0f}, UnownedStuff::unownedCell, nullptr, nullptr,
                                                        nullptr, {}, true, false).get().get();
    if (!newPropRef) {
        logger::error("Failed to create hidden chest.");
        return nullptr;
    }
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

bool ChestManager::IsEmpty(RE::TESObjectREFR* chest) {
    return chest && std::ranges::none_of(chest->GetInventory(), [](const auto& item) {
        return item.second.first > 0;
    });
}

RE::TESObjectREFR* ChestManager::FindNotMatchedChest() const {
    auto& runtimeData = UnownedStuff::unownedCell->GetRuntimeData();
    RE::BSSpinLockGuard locker(runtimeData.spinLock);
    for (const auto& ref : runtimeData.references) {
        if (!ref || ref->IsDeleted()) continue;
        if (ref->GetFormID() == UnownedStuff::unownedChestOGRefID) continue;
        if (ref->GetBaseObject()->GetFormID() != UnownedStuff::unownedChest->GetFormID()) continue;
        if (!rental_state.rented.contains(ref->GetFormID()) &&
            !rental_state.returned.contains(ref->GetFormID()) && IsEmpty(ref.get())) {
            return ref.get();
        }
    }
    return AddChest(GetNoChests());
}

bool ChestManager::Save(SKSE::SerializationInterface* serializationInterface) {
    std::shared_lock lock(register_mutex);
    if (rental_state.rented.size() > std::numeric_limits<std::uint32_t>::max() ||
        rental_state.returned.size() > std::numeric_limits<std::uint32_t>::max() ||
        !serializationInterface->OpenRecord(kDataKey, kSerializationVersion)) return false;

    const auto rental_count = static_cast<std::uint32_t>(rental_state.rented.size());
    if (!serializationInterface->WriteRecordData(rental_count)) return false;
    for (const auto& [chest_refid, client_id] : rental_state.rented) {
        if (!serializationInterface->WriteRecordData(chest_refid) ||
            !serializationInterface->WriteRecordData(client_id)) return false;
    }

    const auto returned_count = static_cast<std::uint32_t>(rental_state.returned.size());
    if (!serializationInterface->WriteRecordData(returned_count)) return false;
    for (const auto chest_refid : rental_state.returned) {
        if (!serializationInterface->WriteRecordData(chest_refid)) return false;
    }
    if (rental_state.clients.size() > std::numeric_limits<std::uint32_t>::max()) return false;
    const auto client_count = static_cast<std::uint32_t>(rental_state.clients.size());
    if (!serializationInterface->WriteRecordData(client_count)) return false;
    for (const auto& [client_id, name] : rental_state.clients) {
        if (name.size() > std::numeric_limits<std::uint32_t>::max()) return false;
        const auto name_length = static_cast<std::uint32_t>(name.size());
        if (!serializationInterface->WriteRecordData(client_id) ||
            !serializationInterface->WriteRecordData(name_length) ||
            !serializationInterface->WriteRecordData(name.data(), name_length)) return false;
    }
    return true;
}

bool ChestManager::Load(SKSE::SerializationInterface* serializationInterface, std::uint32_t length, const std::uint32_t version) {
    RentalState loaded;
    const auto read = [&]<typename T>(T& value) {
        if (length < sizeof(T) || serializationInterface->ReadRecordData(value) != sizeof(T)) return false;
        length -= sizeof(T);
        return true;
    };

    std::uint32_t rental_count;
    constexpr auto rental_size = sizeof(RefID) + sizeof(ContainerizeAPI::ClientID);
    if (!read(rental_count) || length < sizeof(std::uint32_t) ||
        rental_count > (length - sizeof(std::uint32_t)) / rental_size) return false;
    for (std::uint32_t i = 0; i < rental_count; ++i) {
        RefID chest_refid;
        ContainerizeAPI::ClientID client_id;
        if (!read(chest_refid) || !read(client_id)) return false;
        if (serializationInterface->ResolveFormID(chest_refid, chest_refid)) {
            if (!loaded.rented.emplace(chest_refid, client_id).second) return false;
        } else {
            logger::warn("Could not resolve rented chest {:x} during load", chest_refid);
        }
    }

    std::uint32_t returned_count;
    if (!read(returned_count) || returned_count > length / sizeof(RefID)) return false;
    for (std::uint32_t i = 0; i < returned_count; ++i) {
        RefID chest_refid;
        if (!read(chest_refid)) return false;
        if (serializationInterface->ResolveFormID(chest_refid, chest_refid)) {
            loaded.rented.erase(chest_refid);
            loaded.returned.insert(chest_refid);
        } else {
            logger::warn("Could not resolve returned chest {:x} during load", chest_refid);
        }
    }
    if (version == kSerializationVersion) {
        std::uint32_t client_count;
        constexpr auto client_header_size = sizeof(ContainerizeAPI::ClientID) + sizeof(std::uint32_t);
        if (!read(client_count) || client_count > length / client_header_size) return false;
        std::unordered_set<std::string> names;
        for (std::uint32_t i = 0; i < client_count; ++i) {
            ContainerizeAPI::ClientID client_id;
            std::uint32_t name_length;
            if (!read(client_id) || client_id == ContainerizeAPI::containerize_client ||
                !read(name_length) || name_length == 0 || name_length > length) return false;
            std::string name(name_length, '\0');
            if (serializationInterface->ReadRecordData(name.data(), name_length) != name_length) return false;
            length -= name_length;
            if (name.find('\0') != std::string::npos || !names.insert(name).second ||
                !loaded.clients.emplace(client_id, std::move(name)).second) return false;
        }
    }
    for (const auto client_id : loaded.rented | std::views::values) {
        if (client_id != ContainerizeAPI::containerize_client && !loaded.clients.contains(client_id)) return false;
    }
    if (length != 0) return false;

    std::unique_lock lock(register_mutex);
    rental_state = std::move(loaded);
    return true;
}

void ChestManager::Reset() {
    std::unique_lock lock(register_mutex);
    rental_state = {};
}

void ChestManager::RestoreContainerizeChests(const std::vector<RefID>& chest_refids) {
    std::unique_lock lock(register_mutex);
    for (const auto chest_refid : chest_refids) {
        rental_state.rented.insert_or_assign(chest_refid, ContainerizeAPI::containerize_client);
    }
}

bool ChestManager::HasPendingDisposals() const {
    std::shared_lock lock(register_mutex);
    return !rental_state.returned.empty();
}

void ChestManager::ResumeDisposals() {
    std::vector<RE::ObjectRefHandle> chests;
    {
        std::unique_lock lock(register_mutex);
        for (auto it = rental_state.returned.begin(); it != rental_state.returned.end();) {
            if (const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(*it)) {
                chests.push_back(chest->GetHandle());
                ++it;
            } else {
                it = rental_state.returned.erase(it);
            }
        }
    }
    for (const auto& handle : chests) {
        if (const auto chest = handle.get()) ScheduleDisposal(chest.get());
    }
}

void ChestManager::ScheduleDisposal(RE::TESObjectREFR* chest) {
    SKSE::GetTaskInterface()->AddTask([this, handle = chest->GetHandle()] {
        DisposeReturnedChest(handle);
    });
}

void ChestManager::DisposeReturnedChest(const RE::ObjectRefHandle chest_handle) {
    const auto chest = chest_handle.get();
    if (!chest || chest->IsDeleted()) return;
    const auto chest_refid = chest->GetFormID();
    {
        std::shared_lock lock(register_mutex);
        if (!rental_state.returned.contains(chest_refid)) return;
    }

    for (const auto& [item, entry] : chest->GetInventory()) {
        if (entry.first > 0) chest->RemoveItem(item, entry.first, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
    }
    if (IsEmpty(chest.get())) {
        std::unique_lock lock(register_mutex);
        rental_state.returned.erase(chest_refid);
    } else {
        logger::warn("Returned chest {:x} still contains items; submitting it for deletion", chest_refid);
        RE::GarbageCollector::GetSingleton()->Add(chest.get(), true);
    }
}

bool ChestManager::HandleFormDelete(const RefID chest_refid) {
    std::unique_lock lock(register_mutex);
    rental_state.rented.erase(chest_refid);
    return rental_state.returned.erase(chest_refid) > 0 && rental_state.returned.empty();
}

ContainerizeAPI::ClientID ChestManager::RegisterClient(const char* name) {
    if (!name || !*name) return ContainerizeAPI::invalid_client;
    std::unique_lock lock(register_mutex);
    for (const auto& [client_id, registered_name] : rental_state.clients) {
        if (registered_name == name) return client_id;
    }
    const auto last_id = rental_state.clients.empty() ? ContainerizeAPI::containerize_client :
                         rental_state.clients.rbegin()->first;
    if (last_id == std::numeric_limits<ContainerizeAPI::ClientID>::max()) return ContainerizeAPI::invalid_client;
    const auto client_id = last_id + 1;
    rental_state.clients.emplace(client_id, name);
    return client_id;
}

bool ChestManager::Uninstall(const ContainerizeAPI::ClientID client_id) {
    std::vector<RefID> chests;
    {
        std::shared_lock lock(register_mutex);
        if (!rental_state.clients.contains(client_id)) return false;
        for (const auto& [chest_refid, owner] : rental_state.rented) {
            if (owner == client_id) chests.push_back(chest_refid);
        }
    }
    bool success = true;
    for (const auto chest_refid : chests) {
        if (!ReturnChest(client_id, RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid))) success = false;
    }
    return success;
}

bool ChestManager::ReturnChest(const ContainerizeAPI::ClientID client_id, RE::TESObjectREFR* chest) {
    if (!chest) return false;
    const auto empty = IsEmpty(chest);
    {
        std::unique_lock lock(register_mutex);
        const auto lease = rental_state.rented.find(chest->GetFormID());
        if (lease == rental_state.rented.end() || lease->second != client_id) return false;
        rental_state.rented.erase(lease);
        if (!empty) rental_state.returned.insert(chest->GetFormID());
    }
    if (!empty) ScheduleDisposal(chest);
    return true;
}

RE::TESObjectREFR* ChestManager::RentChest(const ContainerizeAPI::ClientID client_id) {
    if (!UnownedStuff::unownedCell) return nullptr;
    std::unique_lock lock(register_mutex);
    if (client_id != ContainerizeAPI::containerize_client && !rental_state.clients.contains(client_id)) return nullptr;
    if (const auto a_chest = FindNotMatchedChest()) {
        if (rental_state.rented.emplace(a_chest->GetFormID(), client_id).second) {
            return a_chest;
        }
        logger::error("Failed to register chest for client {}. Chest ID: {:x}", client_id, a_chest->GetFormID());
        return nullptr;
    }
    return nullptr;
}
