#include "Manager.h"
#include "Papyrus.h"
#include "Animations.h"
#include "CLibUtilsQTR/FormReader.hpp"
#include "CLibUtilsQTR/Tasker.hpp"

// Debug guards to detect improper re-entrant or mixed locking (debug builds only)
// Debug guards to detect improper re-entrant or mixed locking (debug builds only)
#ifndef NDEBUG
namespace {
    struct DebugLockState {
        static thread_local int sharedDepth;
        static thread_local int uniqueDepth;
    };

    thread_local int DebugLockState::sharedDepth = 0;
    thread_local int DebugLockState::uniqueDepth = 0;

    struct DebugSharedLock {
        std::shared_mutex* m;

        explicit DebugSharedLock(std::shared_mutex* mutex) : m(mutex) {
            assert(m && "DebugSharedLock: mutex pointer is null");
            // Cannot take shared if this thread already holds unique
            assert(DebugLockState::uniqueDepth == 0 &&
                   "Attempt to acquire shared lock while holding unique lock on Manager::mutex_ (undefined behavior). Release unique lock first.");
            // Prevent re-entrant shared acquisition
            if (DebugLockState::sharedDepth++ == 0) {
                m->lock_shared();
            } else {
                assert(false &&
                       "Re-entrant shared lock acquisition detected on Manager::mutex_ (undefined behavior). Refactor using NoLock helpers.");
            }
        }

        DebugSharedLock(const DebugSharedLock&) = delete;
        DebugSharedLock& operator=(const DebugSharedLock&) = delete;
        DebugSharedLock(DebugSharedLock&&) = delete;
        DebugSharedLock& operator=(DebugSharedLock&&) = delete;

        ~DebugSharedLock() {
            assert(m && "DebugSharedLock: mutex pointer is null on destruction");
            assert(DebugLockState::sharedDepth > 0 && "Shared depth underflow");
            if (--DebugLockState::sharedDepth == 0) {
                m->unlock_shared();
            }
        }
    };

    struct DebugUniqueLock {
        std::shared_mutex* m;
        bool owns = false;

        explicit DebugUniqueLock(std::shared_mutex* mutex) : m(mutex) {
            assert(m && "DebugUniqueLock: mutex pointer is null");
            // Cannot take unique if shared is currently held
            assert(DebugLockState::sharedDepth == 0 &&
                   "Attempt to acquire unique lock while holding shared lock on Manager::mutex_ (illegal upgrade). Release shared first.");
            // Prevent unique re-entrancy
            assert(DebugLockState::uniqueDepth == 0 &&
                   "Re-entrant unique lock acquisition detected on Manager::mutex_. Refactor to avoid nested mutations.");
            m->lock();
            owns = true;
            DebugLockState::uniqueDepth = 1;
        }

        DebugUniqueLock(const DebugUniqueLock&) = delete;
        DebugUniqueLock& operator=(const DebugUniqueLock&) = delete;
        DebugUniqueLock(DebugUniqueLock&&) = delete;
        DebugUniqueLock& operator=(DebugUniqueLock&&) = delete;

        ~DebugUniqueLock() {
            if (owns) {
                assert(DebugLockState::uniqueDepth == 1 && "Unique depth corruption");
                m->unlock();
                DebugLockState::uniqueDepth = 0;
            }
        }
    };
}

#define SHARED_GUARD DebugSharedLock slock(&mutex_)
#define UNIQUE_GUARD DebugUniqueLock ulock(&mutex_)
#else
#define SHARED_GUARD std::shared_lock slock(mutex_)
#define UNIQUE_GUARD std::unique_lock ulock(mutex_)
#endif
// Avoid Windows GetObject macro conflicts in this file
#undef GetObject

void Manager::TakeBackReal(RE::TESBoundObject* real_obj, RE::TESObjectREFR* chest) {
    const auto unownedChestOG = RE::TESForm::LookupByID<RE::TESObjectREFR>(UnownedStuff::unownedChestOGRefID);
    if (!unownedChestOG) return RaiseMngrErr("MsgBoxCallback unownedChestOG is null");
    if (!Inventory::HasItem(real_obj, unownedChestOG)) {
        return RaiseMngrErr("Real container not found in unownedChestOG");
    }

    const auto temp_pair = std::make_pair(chest->GetFormID(), real_obj->GetFormID());
    bypass_CanBeAdded.insert(temp_pair);
    unownedChestOG->RemoveItem(real_obj, 1, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, chest);
    bypass_CanBeAdded.erase(temp_pair);
}

std::string Manager::GetChestName(const RE::TESObjectREFR* chest) const {
    auto chest_id = chest->GetFormID();
    const auto fake_id = GetFakeID(chest_id);
    if (const auto real_bound = FakeToRealContainer(fake_id)) {
        return renames.contains(fake_id) ? renames.at(fake_id) : real_bound->GetName();
    }
    logger::error("Fake to real container failed for chest ID: {:x}", chest_id);
    return "";
}

bool Manager::ActivateChest(RE::TESObjectREFR* chest) const {
    unownedChest->fullName = GetChestName(chest);
    if (const auto a_obj = chest->GetBaseObject()->As<RE::TESObjectCONT>()) {
        RE::TESObjectCONT::SetOpenState(chest, false, true);
        return a_obj->Activate(chest, player_ref, 0, a_obj, 1);
    }
    logger::error("ActivateChest: Chest is not a container.");
    return false;
}

RE::TESObjectREFR* Manager::GetContainerChest(const RE::TESObjectREFR* a_loc) const {
    if (const auto chest_refid = GetContainerChestID(a_loc->GetFormID())) {
        return RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
    }
    return nullptr;
}

RE::TESObjectREFR* Manager::GetFakeContainerChest(const RE::TESBoundObject* a_fake) const {
    return RE::TESForm::LookupByID<RE::TESObjectREFR>(GetFakeContainerChestID(a_fake->GetFormID()));
}

RE::TESObjectREFR* Manager::GetContainerLocation(const FormID a_fake_id) const {
    SHARED_GUARD;
    const RefID chest_id = GetFakeContainerChestID_NoLock(a_fake_id);
    if (!chest_id) return nullptr;
    const auto real_id = GetRealID_NoLock(chest_id);
    const auto* src = GetContainerSource_NoLock(real_id);
    if (!src) return nullptr;
    const auto it = src->data.find(chest_id);
    if (it == src->data.end()) return nullptr;
    return RE::TESForm::LookupByID<RE::TESObjectREFR>(it->second);
}

uint32_t Manager::GetNoChests() const {
    uint32_t no_chests = 0;
    auto& runtimeData = unownedCell->GetRuntimeData();
    RE::BSSpinLockGuard locker(runtimeData.spinLock);
    for (const auto& ref : runtimeData.references) {
        if (!ref) continue;
        if (ref->IsDeleted()) continue;
        if (ref->GetBaseObject()->GetFormID() == unownedChest->GetFormID()) {
            no_chests++;
        }
    }
    return no_chests;
}

std::vector<RefID> Manager::GetChildChests(const RefID parent_chestID, std::unordered_set<RefID>* parents) {
    if (parents) {
        if (parents->contains(parent_chestID)) {
            logger::critical("GetChildChests: Detected cycle for chest ID: {:x}", parent_chestID);
            return {};
        }
        parents->insert(parent_chestID);
    }

    std::vector<RefID> children;
    {
        SHARED_GUARD;
        for (const auto& [a_chest_id, real_fake] : ChestToFakeContainer) {
            const auto* src = GetContainerSource_NoLock(real_fake.outerKey);
            if (!src) {
                logger::error("Source not found for formid: {:x}", real_fake.outerKey);
                continue;
            }
            if (auto it = src->data.find(a_chest_id);
                a_chest_id != parent_chestID && it != src->data.end() && it->second == parent_chestID) {
                if (!parents || !parents->contains(a_chest_id)) {
                    children.push_back(a_chest_id);
                }
            }
        }
    }
    if (parents) {
        std::vector<RefID> deeper;
        for (const auto child : children) {
            auto children_of_child = GetChildChests(child, parents);
            deeper.insert(deeper.end(), children_of_child.begin(), children_of_child.end());
        }
        children.insert(children.end(), deeper.begin(), deeper.end());
    }
    return children;
}

bool Manager::IsUnownedChest(const RefID refid) const {
    const auto* temp = RE::TESForm::LookupByID<RE::TESObjectREFR>(refid);
    if (!temp) return false;
    const auto base = temp->GetBaseObject();
    return base ? base->GetFormID() == unownedChest->GetFormID() : false;
}

RE::TESObjectREFR* Manager::MakeChest(const RE::NiPoint3 Pos3) const {
    const auto item = unownedChest->As<RE::TESBoundObject>();
    const auto newPropRef = RE::TESDataHandler::GetSingleton()
                            ->CreateReferenceAtLocation(item, Pos3, {0.0f, 0.0f, 0.0f}, unownedCell, nullptr, nullptr,
                                                        nullptr, {}, true, false).get().get();
    logger::info("Created Object! Type: {}, Base ID: {:x}, Ref ID: {:x},",
                 RE::FormTypeToString(item->GetFormType()), item->GetFormID(), newPropRef->GetFormID());
    return newPropRef;
}

RE::TESObjectREFR* Manager::AddChest(const uint32_t chest_no) const {
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

RE::TESObjectREFR* Manager::FindNotMatchedChest() const {
    auto& runtimeData = unownedCell->GetRuntimeData();
    RE::BSSpinLockGuard locker(runtimeData.spinLock);
    for (const auto& ref : runtimeData.references) {
        if (!ref) continue;
        if (ref->GetFormID() == UnownedStuff::unownedChestOGRefID) continue;
        if (ref->GetBaseObject()->GetFormID() != unownedChest->GetFormID()) continue;
        if (!IsChest(ref->GetFormID()) && ref->GetInventory().empty()) {
            return ref.get();
        }
    }
    return AddChest(GetNoChests());
}

void Manager::OpenChestFromMenu(RE::TESObjectREFR* a_chest) {
    if (!closed_menu.empty()) {
        if (!RE::UI::GetSingleton()->IsMenuOpen(closed_menu)) {
            if (!ActivateChest(a_chest)) {
                logger::error("ActivateChest failed for chest: {:x}", a_chest->GetFormID());
            }
        } else {
            clib_utilsQTR::Tasker::GetSingleton()->PushTask(
                [this,a_chest] {
                    SKSE::GetTaskInterface()->AddUITask([this,a_chest] { OpenChestFromMenu(a_chest); });
                }, 500
                );
        }
    }
}

const Source* Manager::GetContainerSource(const FormID real_id) const {
    SHARED_GUARD;
    return GetContainerSource_NoLock(real_id);
}

Source* Manager::GetContainerSource(const FormID real_id) {
    SHARED_GUARD;
    return GetContainerSource_NoLock(real_id);
}

Source* Manager::GetChestSource(const RefID a_chestID) {
    return GetContainerSource(GetRealID(a_chestID));
}

// ================= NoLock helper implementations =================
const Source* Manager::GetContainerSource_NoLock(const FormID real_id) const noexcept {
    for (auto& src : sources) {
        if (src.formid == real_id) return &src;
    }
    return nullptr;
}

Source* Manager::GetContainerSource_NoLock(const FormID real_id) noexcept {
    for (auto& src : sources) {
        if (src.formid == real_id) return &src;
    }
    return nullptr;
}

FormID Manager::GetRealID_NoLock(const RefID chest_id) const noexcept {
    if (const auto it = ChestToFakeContainer.find(chest_id); it != ChestToFakeContainer.end())
        return it->second.
                   outerKey;
    return 0;
}

FormID Manager::GetFakeID_NoLock(const RefID chest_id) const noexcept {
    if (const auto it = ChestToFakeContainer.find(chest_id); it != ChestToFakeContainer.end())
        return it->second.
                   innerKey;
    return 0;
}

RefID Manager::GetFakeContainerChestID_NoLock(const FormID fake_id) const noexcept {
    for (auto& [chest_ref, cont_forms] : ChestToFakeContainer) {
        if (cont_forms.innerKey == fake_id) return chest_ref;
    }
    return 0;
}

RE::TESBoundObject* Manager::GetFakeBound_NoLock(const RefID chest_id) const noexcept {
    if (const auto it = ChestToFakeContainer.find(chest_id); it != ChestToFakeContainer.end()) {
        return RE::TESForm::LookupByID<RE::TESBoundObject>(it->second.innerKey);
    }
    return nullptr;
}

bool Manager::IsChest_NoLock(const RefID a_refid) const noexcept {
    return ChestToFakeContainer.contains(a_refid);
}

Source* Manager::GetChestSource_NoLock(const RefID a_chestID) {
    return GetContainerSource_NoLock(GetRealID_NoLock(a_chestID));
}

bool Manager::HasItemPlusCleanUp(RE::TESBoundObject* item, RE::TESObjectREFR* item_owner) {
    const auto inventory = item_owner->GetInventory();
    if (const auto entry = inventory.find(item); entry == inventory.end()) return false;
    else if (entry->second.first > 0) return true;
    logger::warn("Item count is 0. Removing item.");
    RemoveItem(item_owner, nullptr, item, RE::ITEM_REMOVE_REASON::kRemove);
    return false;
}

void Manager::UpdateFakeWV(RE::TESBoundObject* fake_form, RE::TESObjectREFR* chest_linked, const float weight_ratio) {
    if (!fake_form) return RaiseMngrErr("Fake form is null");
    std::string formtype(RE::FormTypeToString(fake_form->GetFormType()));
    if (formtype == "SCRL") UpdateFakeWV<RE::ScrollItem>(fake_form->As<RE::ScrollItem>(), chest_linked, weight_ratio);
    else if (formtype == "ARMO")
        UpdateFakeWV<RE::TESObjectARMO>(fake_form->As<RE::TESObjectARMO>(), chest_linked,
                                        weight_ratio);
    else if (formtype == "BOOK")
        UpdateFakeWV<RE::TESObjectBOOK>(fake_form->As<RE::TESObjectBOOK>(), chest_linked,
                                        weight_ratio);
    else if (formtype == "INGR")
        UpdateFakeWV<RE::IngredientItem>(fake_form->As<RE::IngredientItem>(), chest_linked,
                                         weight_ratio);
    else if (formtype == "MISC")
        UpdateFakeWV<RE::TESObjectMISC>(fake_form->As<RE::TESObjectMISC>(), chest_linked,
                                        weight_ratio);
    else if (formtype == "WEAP")
        UpdateFakeWV<RE::TESObjectWEAP>(fake_form->As<RE::TESObjectWEAP>(), chest_linked,
                                        weight_ratio);
    else if (formtype == "SLGM")
        UpdateFakeWV<RE::TESSoulGem>(fake_form->As<RE::TESSoulGem>(), chest_linked,
                                     weight_ratio);
    else if (formtype == "ALCH")
        UpdateFakeWV<RE::AlchemyItem>(fake_form->As<RE::AlchemyItem>(), chest_linked,
                                      weight_ratio);
    else if (formtype == "FURN") UpdateFakeWV<RE::TESFurniture>(fake_form->As<RE::TESFurniture>(), chest_linked,
                                                                weight_ratio);
    else RaiseMngrErr(std::format("Form type not supported: {}", formtype));
}


void Manager::UpdateFakeWV(RE::TESBoundObject* fake_form) {
    const auto chestID = GetFakeContainerChestID(fake_form->GetFormID());
    const auto chestRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(chestID);
    if (!chestRef) {
        logger::error("Chest ref not found.");
        return;
    }
    const auto src = GetChestSource(chestID);
    if (!src) {
        logger::error("Source not found.");
        return;
    }
    UpdateFakeWV(fake_form, chestRef, src->weight_ratio);
}

void Manager::HandleFormDelete_(const RefID chest_refid) {
    auto real_formid = GetRealID(chest_refid);
    if (const auto real_item = RE::TESForm::LookupByID<RE::TESBoundObject>(real_formid)) {
        const auto msg =
            std::format("Your container with name {} was deleted by the game. Will try to return your items now.",
                        real_item->GetName());
        MsgBoxesNotifs::InGame::CustomMsg(msg);
    } else {
        const auto msg =
            std::format("Your container with formid {:x} was deleted by the game. Will try to return your items now.",
                        real_formid);
        MsgBoxesNotifs::InGame::CustomMsg(msg);
    }

    if (const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid)) {
        if (DeRegister(chest, player_ref)) {
            return;
        }
    }

    MsgBoxesNotifs::InGame::CustomMsg("Something went wrong while returning your items.");
    RaiseMngrErr(std::format("Failed to deregister chest {}", chest_refid));
}

void Manager::RaiseMngrErr(const std::string& err_msg_) {
    logger::error("{}", err_msg_);
    MsgBoxesNotifs::InGame::CustomMsg(err_msg_);
    MsgBoxesNotifs::InGame::GeneralErr();
    Uninstall();
}

void Manager::InitFailed() {
    logger::critical("Failed to initialize Manager.");
    MsgBoxesNotifs::InGame::InitErr();
    Uninstall();
}

FormID Manager::CreateFakeContainer(RE::TESBoundObject* container, const RefID connected_chest, RE::ExtraDataList* el) {
    std::string formtype(RE::FormTypeToString(container->GetFormType()));
    if (formtype == "SCRL") { return CreateFakeContainer(container->As<RE::ScrollItem>(), connected_chest, el); }
    if (formtype == "ARMO") { return CreateFakeContainer(container->As<RE::TESObjectARMO>(), connected_chest, el); }
    if (formtype == "BOOK") { return CreateFakeContainer(container->As<RE::TESObjectBOOK>(), connected_chest, el); }
    if (formtype == "INGR") { return CreateFakeContainer(container->As<RE::IngredientItem>(), connected_chest, el); }
    if (formtype == "MISC") { return CreateFakeContainer(container->As<RE::TESObjectMISC>(), connected_chest, el); }
    if (formtype == "WEAP") { return CreateFakeContainer(container->As<RE::TESObjectWEAP>(), connected_chest, el); }
    if (formtype == "SLGM") { return CreateFakeContainer(container->As<RE::TESSoulGem>(), connected_chest, el); }
    if (formtype == "ALCH") { return CreateFakeContainer(container->As<RE::AlchemyItem>(), connected_chest, el); }
    if (formtype == "FURN") { return CreateFakeContainer(container->As<RE::TESFurniture>(), connected_chest, el); }
    logger::error("Form type not supported: {}", formtype);
    return 0;
}

RE::TESBoundObject* Manager::FakePlacement_Sub_Sub(const RefID chestID) {
    // Read-only phase under shared lock
    FormID real_formid;
    FormID fakeid_old;
    {
        SHARED_GUARD;
        real_formid = GetRealID_NoLock(chestID);
        fakeid_old = GetFakeID_NoLock(chestID);
    }
    const auto real_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(real_formid);
    if (!real_bound) {
        logger::error("FakePlacement_Sub_Sub: real_bound null {:x}", real_formid);
        return nullptr;
    }
    const FormID fakeid_new = CreateFakeContainer(real_bound, chestID, nullptr);
    if (!fakeid_new) return nullptr;

    // Mutations under unique lock
    {
        UNIQUE_GUARD;
        const auto it = ChestToFakeContainer.find(chestID);
        if (it != ChestToFakeContainer.end()) {
            it->second.innerKey = fakeid_new;
        }
    }
    if (const auto itFav = std::ranges::find(external_favs, fakeid_old); itFav != external_favs.end()) {
        external_favs.erase(itFav);
        external_favs.push_back(fakeid_new);
    }
    if (renames.contains(fakeid_old) && fakeid_new != fakeid_old) {
        renames[fakeid_new] = renames.at(fakeid_old);
        renames.erase(fakeid_old);
    }

    const auto DFT = DynamicFormTracker::GetSingleton();
    if (DFT->IsActive(fakeid_old)) {
        DFT->SetInactive(fakeid_old);
        DFT->Reserve(real_formid, clib_util::editorID::get_editorID(real_bound), fakeid_old);
    }

    return RE::TESForm::LookupByID<RE::TESBoundObject>(fakeid_new);
}

void Manager::FakePlacement_Sub(RE::TESObjectREFR* chest, RE::TESObjectREFR* saved_loc) {
    if (!chest || !saved_loc) return RaiseMngrErr("FakePlacement_Sub: null refs");
    const auto chestID = chest->GetFormID();
    const auto fake_bound_new = FakePlacement_Sub_Sub(chestID);
    if (!fake_bound_new) return RaiseMngrErr("Fake bound creation failed");

    const auto real_formid = GetRealID(chestID);
    const auto real_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(real_formid);

    RE::ExtraDataList* xList_fake = nullptr;
    if (real_bound) {
        const auto inventory = chest->GetInventory();
        if (inventory.contains(real_bound)) {
            xList_fake = xData::ConstructExtraDataList();
            const auto real_entry = inventory.at(real_bound).second.get();
            const auto xList_real = real_entry && real_entry->extraLists && !real_entry->extraLists->empty()
                                        ? real_entry->extraLists->front()
                                        : nullptr;
            if (!xData::UpdateExtras(xList_real, xList_fake)) {
                logger::warn("Failed to copy extras to fake container");
            }
        } else {
            logger::critical("Real container not found in chest inventory!");
        }
    }
    saved_loc->AddObjectToContainer(fake_bound_new, xList_fake, 1, nullptr);
}

void Manager::FakePlacement(RefID saved_loc, const RefID chest_refID, RE::TESObjectREFR* external_cont) {
    if (Settings::is_pre_0_10_0) {
        if (chest_refID == saved_loc) saved_loc = player_refid;
    }
    if (!external_cont && saved_loc != player_refid && !IsChest(saved_loc)) return;

    if (IsRealContainer(external_cont)) {
        logger::critical("saved_loc should not be realcontainer out in the world!");
        return;
    }

    const auto saved_loc_ref = external_cont ? external_cont : RE::TESForm::LookupByID<RE::TESObjectREFR>(saved_loc);
    if (!saved_loc_ref) return RaiseMngrErr("saved_loc_ref not found");
    const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refID);
    if (!chest) return RaiseMngrErr("Chest not found");

    RE::TESBoundObject* fake_bound = GetFakeBound(chest_refID);
    const bool fake_nonexistent = !fake_bound || !HasItemPlusCleanUp(fake_bound, saved_loc_ref);
    if (fake_nonexistent) {
        FakePlacement_Sub(chest, saved_loc_ref);
    }

    const auto realID = GetRealID(chest_refID);
    auto fakeid = GetFakeID(chest_refID);
    if (!fake_nonexistent && !xData::UpdateExtrasInInventory(chest, realID, saved_loc_ref, fakeid)) {
        logger::error("Failed to update extras");
    }

    const auto src = GetContainerSource(realID);
    if (!src) return RaiseMngrErr("Could not find source for container");
    fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fakeid);
    if (!fake_bound) return RaiseMngrErr("Fake bound not found");
    UpdateFakeWV(fake_bound, chest, src->weight_ratio);

    if (std::ranges::find(external_favs, fakeid) != external_favs.end()) {
        if (const auto inventory_changes = saved_loc_ref->GetInventoryChanges()) {
            if (const auto entry_list = inventory_changes->entryList) {
                const auto it2 = std::ranges::find_if(*entry_list, [fakeid](const auto& entry) {
                    return entry && entry->object && entry->object->GetFormID() == fakeid;
                });
                if (it2 != entry_list->end() && !(*it2)->IsFavorited()) {
                    Inventory::FavoriteItem(*it2, inventory_changes);
                }
            }
        }
    }
    if (Settings::other_settings[Settings::otherstuffKeys[1]]) RemoveCarryWeightBoost(fakeid, saved_loc_ref);
}

void Manager::FakePlacementCeption(const RefID chest_ref, std::vector<RefID>& ha) {
    if (std::ranges::find(ha, chest_ref) != ha.end()) return;
    ha.push_back(chest_ref);
    logger::info("-------------------chest_ref: {:x} -------------------", chest_ref);
    for (const auto& connected_chest : GetChildChests(chest_ref, nullptr)) {
        logger::info("Connected chest: {:x}", connected_chest);
        FakePlacementCeption(connected_chest, ha);
    }
    RefID saved_loc = 0;
    bool error = false;
    {
        SHARED_GUARD;
        const auto src = GetChestSource_NoLock(chest_ref);
        if (!src) {
            logger::error("Could not find source for container {:x}", chest_ref);
            error = true;
        } else {
            const auto it = src->data.find(chest_ref);
            if (it == src->data.end()) {
                logger::error("Source data missing chest_ref {:x}", chest_ref);
                error = true;
            } else {
                saved_loc = it->second;
            }
        }
    }

    if (error) return RaiseMngrErr("Error in FakePlacementCeption");

    FakePlacement(saved_loc, chest_ref);
    logger::info("-------------------chest_ref: {:x} DONE -------------------", chest_ref);
}

void Manager::RemoveCarryWeightBoost(const FormID item_formid, RE::TESObjectREFR* inventory_owner) {
    if (!inventory_owner) return;
    const auto item_obj = RE::TESForm::LookupByID<RE::TESBoundObject>(item_formid);
    if (!item_obj) return RaiseMngrErr("Item not found");

    const auto inventory = inventory_owner->GetInventory();
    const auto it = inventory.find(item_obj);
    if (it == inventory.end()) {
        logger::error("Item not found in inventory_owner");
        return;
    }
    if (const auto enchantment = it->second.second->GetEnchantment()) {
        for (const auto& effect : enchantment->effects) {
            if (effect->baseEffect->data.primaryAV == RE::ActorValue::kCarryWeight) {
                effect->effectItem.magnitude = std::min<float>(effect->effectItem.magnitude, 0);
            }
        }
    }
}

bool Manager::HandleRegistration(RE::TESObjectREFR* a_item) {
    const auto master_formid = a_item->GetBaseObject()->GetFormID();

    const auto container_refid = a_item->GetFormID();
    if (!IsARegistry(container_refid)) {
        const auto src = GetContainerSource(master_formid);
        if (!src) {
            logger::error("Source not found for container with refid: {:x}", a_item->GetFormID());
            return false;
        }
        const auto ChestObjRef = FindNotMatchedChest();
        if (!ChestObjRef) return false;
        const auto ChestRefID = ChestObjRef->GetFormID();
        logger::info("Matched chest {:x} with container {:x}", ChestRefID, container_refid);
        const auto fake_formid = CreateFakeContainer(a_item->GetObjectReference(), ChestRefID, nullptr);
        if (!fake_formid) return false;
        RE::ExtraDataList* xList_copy = xData::ConstructExtraDataList();
        if (!xData::UpdateExtras(&a_item->extraList, xList_copy)) {
            logger::warn("Failed to copy extra data list.");
        }
        ChestObjRef->AddObjectToContainer(a_item->GetBaseObject(), xList_copy, 1, nullptr);
        // TODO: test
        if (const auto& initial_items_map = src->initial_items; !initial_items_map.empty()) {
            if (const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(ChestRefID)) {
                for (const auto& [item, count] : initial_items_map) {
                    if (count <= 0) continue;
                    auto* bound = RE::TESForm::LookupByID<RE::TESBoundObject>(item);
                    if (!bound) continue;
                    chest->AddObjectToContainer(bound, nullptr, count, nullptr);
                }
            } else logger::error("Chest not found for initial items addition.");
        }
        return Register_Sub(master_formid, fake_formid, ChestRefID, container_refid);
    }
    const auto chest_refid = GetContainerChestID(container_refid);
    const auto real_cont_id = GetRealID(chest_refid);
    const auto real_cont_editorid = FormReader::GetEditorID(real_cont_id);
    if (real_cont_editorid.empty()) {
        RaiseMngrErr("Failed to get editorid of real container.");
        return false;
    }
    auto* DFT = DynamicFormTracker::GetSingleton();
    if (const auto fake_cont_id = DFT->Fetch(real_cont_id, real_cont_editorid, chest_refid); !fake_cont_id) {
        logger::info("Fake container NOT found in DFT.");
        if (!FakePlacement_Sub_Sub(chest_refid)) {
            RaiseMngrErr("Failed to create fake container.");
            return false;
        }
    } else {
        DFT->EditCustomID(fake_cont_id, chest_refid);
    }
    return true;
}

bool Manager::DeRegister(RE::TESObjectREFR* chest, RE::TESObjectREFR* transfer_dest) {
    if (!chest) return false;
    const auto chestID = chest->GetFormID();
    const auto fake_bound = GetFakeBound(chestID);
    if (!fake_bound) {
        logger::critical("DeRegister: fake_bound null for chest {:x}", chestID);
        return false;
    }
    const auto fake_loc = GetContainerLocation(fake_bound->GetFormID());

    if (!DeRegister_Sub(GetRealID(chestID), chestID)) {
        logger::critical("Failed to deregister chestID: {:x}", chestID);
        RaiseMngrErr("Failed to deregister chest.");
        return false;
    }

    if (fake_loc) {
        fake_loc->RemoveItem(fake_bound, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
    }
    for (auto& [fst,snd] : chest->GetInventory()) {
        chest->RemoveItem(fst, snd.first, RE::ITEM_REMOVE_REASON::kRemove, nullptr, transfer_dest);
    }
    if (!chest->GetInventory().empty()) {
        logger::critical("Chest inventory not empty after deregistration!");
        return false;
    }
    return true;
}

std::string Manager::GetWeightText_(RE::TESObjectREFR* a_chest) {
    const auto chest_id = a_chest->GetFormID();
    if (const auto a_real_id = GetRealID(chest_id)) {
        const auto real = RE::TESForm::LookupByID<RE::TESBoundObject>(a_real_id);
        const auto src = GetContainerSource(a_real_id);
        if (real && src && src->capacity > 0) {
            const auto a_weight = std::max(0.f, a_chest->GetWeightInContainer() - real->GetWeight());
            return GetWeightText(a_weight * src->weight_ratio, src->capacity);
        }
    }
    return "";
}

std::string Manager::GetWeightText(const float weight, const float capacity) {
    std::ostringstream stream1;
    stream1 << std::fixed << std::setprecision(2) << weight;
    std::ostringstream stream2;
    stream2 << std::fixed << std::setprecision(2) << capacity;
    return fmt::format("{}/{}", stream1.str(), stream2.str());
}

bool Manager::Register_Sub(const FormID master_formID, const FormID fake_formID, RefID chest_refID, RefID loc_refID) {
    UNIQUE_GUARD;
    Source* src = GetContainerSource_NoLock(master_formID);
    if (!src) return false;
    if (!src->data.insert({chest_refID, loc_refID}).second) return false;
    if (!ChestToFakeContainer.insert({chest_refID, {.outerKey = master_formID, .innerKey = fake_formID}}).second) {
        src->data.erase(chest_refID);
        return false;
    }
    return true;
}

bool Manager::DeRegister_Sub(const FormID master_formID, const RefID chest_refID) {
    UNIQUE_GUARD;
    Source* src = GetContainerSource_NoLock(master_formID);
    if (!src) return false;
    if (!src->data.erase(chest_refID)) return false;
    if (!ChestToFakeContainer.erase(chest_refID)) return false;
    return true;
}

FormID Manager::GetFakeID(const RefID chest_id) const {
    SHARED_GUARD;
    return GetFakeID_NoLock(chest_id);
}

FormID Manager::GetRealID(const RefID chest_id) const {
    SHARED_GUARD;
    return GetRealID_NoLock(chest_id);
}

RefID Manager::GetContainerChestID(const RefID a_loc_refid) const {
    SHARED_GUARD;
    for (const auto& src : sources) {
        for (const auto& [chest_refid, cont_refid] : src.data) {
            if (cont_refid == a_loc_refid) return chest_refid;
        }
    }
    return 0;
}

RefID Manager::GetFakeContainerChestID(const FormID fake_id) const {
    SHARED_GUARD;
    return GetFakeContainerChestID_NoLock(fake_id);
}

RE::TESBoundObject* Manager::GetFakeBound(const RefID chest_id) const {
    SHARED_GUARD;
    return GetFakeBound_NoLock(chest_id);
}

RE::TESBoundObject* Manager::GetRealBound(const RefID chest_id) const {
    return RE::TESForm::LookupByID<RE::TESBoundObject>(GetRealID(chest_id));
}

void Manager::UpdateLoc_Private(const RefID chestID, const RefID loc_id) {
    if (!IsChest(chestID)) {
        logger::error("Chest ID not found in ChestToFakeContainer.");
        return;
    }
    {
        UNIQUE_GUARD;
        const auto real_id = GetRealID_NoLock(chestID);
        Source* src = GetContainerSource_NoLock(real_id);
        if (!src) {
            logger::error("UpdateLoc_Private: source null");
            return;
        }
        if (src->data.contains(chestID)) src->data.at(chestID) = loc_id;
    }
    UpdateFakeWV(GetFakeBound(chestID));
}

void Manager::TransferOnUse(const RefID a_chestID) const {
    if (const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(a_chestID)) {
        const auto a_inv = chest->GetInventoryCounts();
        const auto a_realID = GetRealID(a_chestID);
        std::vector<std::pair<FormID, Count>> a_cache;
        for (const auto& [item, count] : a_inv) {
            auto a_formid = item->GetFormID();
            auto a_count = count;
            if (IsFakeContainer(a_formid)) {
                TransferOnUse(GetFakeContainerChestID(a_formid));
                continue;
            }
            if (a_realID == a_formid) {
                a_count -= 1;
            }
            if (a_count > 0) {
                a_cache.emplace_back(a_formid, a_count);
                chest->RemoveItem(item, a_count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, player_ref);
            }
        }
        transfer_cache[a_chestID] = std::move(a_cache);
    }
}

bool Manager::IsARegistry(const RefID registry) const {
    SHARED_GUARD;
    for (const auto& src : sources) {
        for (const auto& cont_ref : src.data | std::views::values) {
            if (cont_ref == registry) return true;
        }
    }
    return false;
}

void Manager::Gateway(const int result, const RE::ObjectRefHandle& a_current_container) {
    if (result != 0 && result != 1) {
        logger::error("Unexpected callback integer!");
        return;
    }
    const auto a_chest_handle = a_current_container.get();
    const auto a_chest = a_chest_handle ? GetContainerChest(a_chest_handle.get()) : nullptr;
    if (!a_chest) {
        logger::warn("Current container is null.");
    } else if (result) {
        RenameCallback(GetFakeBound(a_chest->GetFormID()));
    } else if (!ActivateChest(a_chest)) {
        reals_to_takeback.clear();
        queued_chests.clear();
        Animations::SendAnimEvent(3, nullptr);
        logger::warn("Chest not found.");
    }
}

void Manager::Init() {
    player_ref = RE::PlayerCharacter::GetSingleton()->As<RE::TESObjectREFR>();
    bool init_failed = false;
    sources = Settings::LoadSources();
    if (sources.empty()) {
        logger::error("No sources found.");
        return InitFailed();
    }

    std::unordered_set<std::uint32_t> encounteredFormIDs;

    for (auto& src : sources) {
        const auto form_ = FormReader::GetFormByID(src.formid, src.editorid);
        if (const auto bound_ = src.GetBoundObject(); !form_ || !bound_) {
            init_failed = true;
            logger::error("Missing source: {:x}, {}", src.formid, src.editorid);
            break;
        }
        auto formtype_ = RE::FormTypeToString(form_->GetFormType());
        if (std::string formtypeString(formtype_); !Settings::AllowedFormTypes.contains(formtypeString)) {
            init_failed = true;
            MsgBoxesNotifs::InGame::FormTypeErr(form_->GetFormID());
            logger::error("Invalid source type: {}", formtype_);
            break;
        }
        if (!encounteredFormIDs.insert(src.formid).second) {
            logger::error("Duplicate formid found: {}", src.formid);
            init_failed = true;
        }
    }

    const auto unownedChestOG = RE::TESForm::LookupByID<RE::TESObjectREFR>(0x000EA29A);
    unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(UnownedStuff::unownedChestFormID);
    unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000EA28B);
    if (!unownedChestOG || unownedChestOG->GetBaseObject()->GetFormID() != unownedChest->GetFormID() || !unownedCell ||
        !unownedChest || !unownedChest->As<RE::TESBoundObject>()) {
        logger::error("Missing unowned chest/cell");
        init_failed = true;
    }
    if (Settings::is_pre_0_7_1 && unownedChestOG) {
        for (auto& [fst,snd] : unownedChestOG->GetInventory()) {
            unownedChestOG->RemoveItem(fst, snd.first, RE::ITEM_REMOVE_REASON::kRemove, nullptr, player_ref);
            if (fst->IsDynamicForm())
                player_ref->RemoveItem(fst, snd.first, RE::ITEM_REMOVE_REASON::kRemove, nullptr,
                                       nullptr);
        }
    }
    if (init_failed) return InitFailed();
    logger::info("Manager initialized.");
}

void Manager::Reset() {
    logger::info("Resetting manager...");
    {
        UNIQUE_GUARD;
        for (auto& src : sources) src.data.clear();
        ChestToFakeContainer.clear();
    }
    external_favs.clear();
    renames.clear();
    handled_external_conts.clear();
    Clear();
    isUninstalled.store(false);
    logger::info("Manager reset.");
}

void Manager::SendData() {
    logger::info("--------Sending data---------");
    Clear();
    int no_of_container = 0;
    bool error = false;
    {
        SHARED_GUARD;
        for (auto& src : sources) {
            if (error) break;
            for (const auto& [chest_ref, cont_ref] : src.data) {
                no_of_container++;
                if (!chest_ref) {
                    logger::error("Chest refid is null");
                    error = true;
                    break;
                }
                auto itChest = ChestToFakeContainer.find(chest_ref);
                if (itChest == ChestToFakeContainer.end()) continue;
                auto fake_formid = itChest->second.innerKey;
                bool is_equipped_x = false;
                bool is_favorited_x = false;
                if (cont_ref == player_refid) {
                    const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
                    is_equipped_x = Inventory::IsEquipped(fake_bound);
                    is_favorited_x = Inventory::IsFavorited(fake_bound, player_ref);
                    if (const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref); !chest) {
                        logger::error("Chest not found");
                        error = true;
                        break;
                    }
                } else if (std::ranges::find(external_favs, fake_formid) != external_favs.end()) {
                    is_favorited_x = true;
                }
                const auto rename_ = renames.contains(fake_formid) ? renames.at(fake_formid) : "";
                FormIDX fake_container_x(itChest->second.innerKey, is_equipped_x, is_favorited_x, rename_);
                SetData({src.formid, chest_ref}, {fake_container_x, cont_ref});
            }
        }
    }

    if (error) {
        return RaiseMngrErr();
    }

    logger::info("Data sent. Number of containers: {}", no_of_container);
}

void Manager::ReceiveData() {
    logger::info("--------Receiving data---------");
    std::map<RefID, std::pair<bool, bool>> chest_equipped_fav;
    std::map<RefID, FormFormID> unmathced_chests;
    for (const auto& [realcontForm_chestRef, fakecontForm_contRef] : m_Data) {
        auto [realcontFormID, chestRefID] = realcontForm_chestRef;
        auto [fakecontForm_info, locRefID] = fakecontForm_contRef;
        if (Settings::is_pre_0_10_0 && locRefID == chestRefID) locRefID = player_refid;
        if (Register_Sub(realcontFormID, fakecontForm_info.id, chestRefID, locRefID)) {
            if (!fakecontForm_info.name.empty()) renames[fakecontForm_info.id] = fakecontForm_info.name;
            if (locRefID == player_refid)
                chest_equipped_fav[chestRefID] = {
                    fakecontForm_info.equipped, fakecontForm_info.favorited};
            else if (fakecontForm_info.favorited) external_favs.push_back(fakecontForm_info.id);
        } else {
            unmathced_chests[chestRefID] = {.outerKey = realcontFormID, .innerKey = fakecontForm_info.id};
        }
    }
    for (const auto& [chestRef_, RealFakeForm_] : unmathced_chests) {
        auto [realcontFormID, fakecontFormID] = RealFakeForm_;
        logger::warn("FormID {:x} not found in sources.", realcontFormID);
        if (Settings::other_settings[Settings::otherstuffKeys[0]]) {
            MsgBoxesNotifs::InGame::ProblemWithContainer(realcontFormID);
        }
        if (const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fakecontFormID)) {
            player_ref->RemoveItem(fake_bound, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
        }
        logger::info("Trying to retrieve items from chest");
        if (const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chestRef_)) {
            for (auto& [fst,snd] : chest->GetInventory()) {
                chest->RemoveItem(fst, snd.first, RE::ITEM_REMOVE_REASON::kRemove, nullptr, player_ref);
                if (fst->GetFormID() == fakecontFormID)
                    player_ref->RemoveItem(
                        fst, snd.first, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            }
            if (!chest->GetInventory().empty()) {
                logger::critical("Chest still has items in it. Degistering failed");
                MsgBoxesNotifs::InGame::CustomMsg("Items might not have been retrieved successfully.");
            }
        }
        m_Data.erase({RealFakeForm_.outerKey, chestRef_});
    }
    #ifndef NDEBUG
    Print();
    #endif
    auto* DFT = DynamicFormTracker::GetSingleton();
    std::vector<RefID> handled_already;
    std::vector<RefID> all_chestIDs;
    {
        SHARED_GUARD;
        for (const auto& chest_ref : ChestToFakeContainer | std::views::keys) all_chestIDs.push_back(chest_ref);
    }
    for (const auto& a_chestID : all_chestIDs) {
        if (std::ranges::find(handled_already, a_chestID) != handled_already.end()) continue;
        FakePlacementCeption(a_chestID, handled_already);
        auto a_realID = GetRealID(a_chestID);
        const auto real_editorid = FormReader::GetEditorID(a_realID);
        if (real_editorid.empty()) {
            logger::critical("Real container with formid {:x} has no editorid.", a_realID);
            return RaiseMngrErr("Real container has no editorid.");
        }
        const auto a_fakeID = GetFakeID(a_chestID);
        DFT->Reserve(a_realID, real_editorid, a_fakeID);
    }
    all_chestIDs.clear();
    handled_already.clear();
    const auto inventory_changes = player_ref->GetInventoryChanges();
    const auto entries = inventory_changes->entryList;
    for (auto it = entries->begin(); it != entries->end(); ++it) {
        if (const auto a_entry = *it; a_entry && a_entry->object) {
            auto fake_formid = a_entry->object->GetFormID();
            if (IsFakeContainer(fake_formid)) {
                const auto a_chestID = GetFakeContainerChestID(fake_formid);
                if (chest_equipped_fav.contains(a_chestID)) {
                    const auto& [is_equipped_x,is_faved_x] = chest_equipped_fav.at(a_chestID);
                    if (a_entry->IsWorn()) {
                        RE::ActorEquipManager::GetSingleton()->UnequipObject(
                            RE::PlayerCharacter::GetSingleton(), a_entry->object,
                            a_entry->extraLists && !a_entry->extraLists->empty()
                                ? a_entry->extraLists->front()
                                : nullptr, 1,
                            nullptr, false, false, false, false);
                    }
                    if (is_equipped_x) {
                        Inventory::EquipItem(a_entry);
                    }
                    if (is_faved_x && !a_entry->IsFavorited()) {
                        Inventory::FavoriteItem(a_entry, inventory_changes);
                    }
                }
            }
        } else logger::error("Entry or object null in ReceiveData fave-equip.");
    }

    std::vector<std::tuple<FormID, RefID, float>> pendingWV;
    {
        SHARED_GUARD;
        for (auto& source : sources) {
            for (auto dyn_formid : DFT->GetFormSet(source.formid, source.editorid)) {
                const auto editorid = source.editorid.empty()
                                          ? FormReader::GetEditorID(source.formid)
                                          : source.editorid;
                DFT->Reserve(source.formid, editorid, dyn_formid);
            }
            for (const auto& chest_refid : source.data | std::views::keys) {
                if (const auto fake_formid = GetFakeID_NoLock(chest_refid)) {
                    pendingWV.emplace_back(fake_formid, chest_refid, source.weight_ratio);
                }
            }
        }
    }

    for (const auto& [fake_formid, chest_refid, weight_ratio] : pendingWV) {
        const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
        if (const auto chest_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid); fake_bound && chest_ref) {
            UpdateFakeWV(fake_bound, chest_ref, weight_ratio);
        } else {
            logger::error("ReceiveData: missing fake_bound or chest_ref for chest {:x}", chest_refid);
        }
    }

    logger::info("Deleting unused fake forms from bank.");
    DFT->DeleteInactives();
    if (DFT->GetNDeleted() > 0) {
        logger::warn("ReceiveData: Deleted forms exist. User is required to restart.");
        MsgBoxesNotifs::InGame::CustomMsg(
            "It seems the configuration has changed from your previous session"
            " that requires you to restart the game."
            "DO NOT IGNORE THIS:"
            "1. Save your game."
            "2. Exit the game."
            "3. Restart the game."
            "4. Load the saved game."
            "JUST DO IT! NOW! BEFORE DOING ANYTHING ELSE!");
    }
    logger::info("--------Receiving data done---------");
}

void Manager::RenameContainer(const std::string& new_name, RE::TESBoundObject* a_fake) {
    const auto fake_formid = a_fake->GetFormID();
    const std::string formtype(RE::FormTypeToString(a_fake->GetFormType()));
    if (formtype == "SCRL") Rename(new_name, a_fake->As<RE::ScrollItem>());
    else if (formtype == "ARMO") Rename(new_name, a_fake->As<RE::TESObjectARMO>());
    else if (formtype == "BOOK") Rename(new_name, a_fake->As<RE::TESObjectBOOK>());
    else if (formtype == "INGR") Rename(new_name, a_fake->As<RE::IngredientItem>());
    else if (formtype == "MISC") Rename(new_name, a_fake->As<RE::TESObjectMISC>());
    else if (formtype == "WEAP") Rename(new_name, a_fake->As<RE::TESObjectWEAP>());
    else if (formtype == "SLGM") Rename(new_name, a_fake->As<RE::TESSoulGem>());
    else if (formtype == "ALCH") Rename(new_name, a_fake->As<RE::AlchemyItem>());
    else if (formtype == "FURN") Rename(new_name, a_fake->As<RE::TESFurniture>());
    else logger::warn("Form type not supported: {}", formtype);
    {
        UNIQUE_GUARD;
        renames[fake_formid] = new_name;
    }
    RE::ExtraDataList* xList = nullptr;
    RE::TESObjectREFR* a_containermenu_owner = nullptr;
    if (const auto a_current_container = GetContainerLocation(fake_formid)) {
        if (a_current_container->HasContainer()) {
            a_containermenu_owner = a_current_container;
            auto inv = a_current_container->GetInventory();
            if (const auto it = inv.find(a_fake); it != inv.end()) {
                xList = it->second.second && it->second.second->extraLists && !it->second.second->extraLists->empty()
                            ? it->second.second->extraLists->front()
                            : nullptr;
            }
        } else {
            xList = &a_current_container->extraList;
        }
    }
    if (xList) xData::AddTextDisplayData(xList, new_name);
    if (const auto ui = RE::UI::GetSingleton();
        a_containermenu_owner &&
        (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) || ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME))) {
        RE::SendUIMessage::SendInventoryUpdateMessage(a_containermenu_owner, nullptr);
    }
}

void Manager::UpdateLoc(const FormID fakeID, const RefID loc_id) {
    const auto chestID = GetFakeContainerChestID(fakeID);
    UpdateLoc_Private(chestID, loc_id);
}

Count Manager::CanBeAdded(const RE::TESBoundObject* a_item, const Count a_count, RefID a_chestID) {
    if (!a_item) return 0;
    if (bypass_CanBeAdded.contains({a_chestID, a_item->GetFormID()})) {
        return a_count;
    }
    if (const auto item_id = a_item->GetFormID(); IsFakeContainer(item_id)) {
        if (a_chestID == GetFakeContainerChestID(item_id)) {
            logger::warn("Avoided transferring fake container into its own chest.");
            return 0;
        }
        if (const auto other_chest = GetFakeContainerChest(a_item)) {
            std::unordered_set<RefID> visited;
            for (const auto& a_child_chest : GetChildChests(other_chest->GetFormID(), &visited)) {
                if (a_child_chest == a_chestID) {
                    return 0;
                }
            }
        }
    }
    if (a_item->GetWeight() < 0.001f) return a_count;
    const auto chestRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(a_chestID);
    if (!chestRef) {
        logger::error("Chest ref not found.");
        return 0;
    }
    SHARED_GUARD;
    const auto itChest = ChestToFakeContainer.find(a_chestID);
    if (itChest == ChestToFakeContainer.end()) return 0;
    const auto src = GetContainerSource_NoLock(itChest->second.outerKey);
    if (!src) {
        logger::error("Source not found.");
        return 0;
    }
    if (src->weight_ratio < 0.00001f) return a_count;
    if (src->capacity == 0.f) return a_count;
    const auto remaining_capacity = src->capacity - chestRef->GetWeightInContainer() * src->weight_ratio;
    const auto item_weight = a_item->GetWeight() * src->weight_ratio;
    const auto can_be_added = static_cast<Count>(remaining_capacity / (item_weight + EPSILON));
    return std::max(0, std::min(can_be_added, a_count));
}

void Manager::OnChestExit(RE::TESObjectREFR* a_chest) {
    const auto chest_id = a_chest->GetFormID();
    const bool next_menu_is_chest = containermenu_owner && IsChest(containermenu_owner->GetFormID());
    const auto real_bound = GetRealBound(chest_id);
    if (reals_to_takeback.contains(chest_id)) {
        TakeBackReal(real_bound, a_chest);
        reals_to_takeback.erase(chest_id);
        const auto fake_bound = GetFakeBound(chest_id);
        if (fake_bound && real_bound) fake_bound->formFlags = real_bound->formFlags;
        if (Settings::other_settings.at(Settings::otherstuffKeys[2]) && queued_chests.empty()) {
            if (closed_menu == RE::ContainerMenu::MENU_NAME) {
                if (containermenu_owner) {
                    SKSE::GetTaskInterface()->AddUITask(
                        [this] {
                            if (containermenu_owner && IsChest(containermenu_owner->GetFormID())) {
                                if (!ActivateChest(containermenu_owner.get())) {
                                    logger::error("ActivateChest failed for containermenu_owner: {:x}",
                                                  containermenu_owner->GetFormID());
                                }
                            } else if (containermenu_owner) {
                                containermenu_owner->OpenContainer(0);
                            }
                            containermenu_owner.reset();
                        });
                } else logger::error("containermenu_owner is null in OnChestExit");
            } else {
                Menu::OpenMenu(closed_menu);
            }
            closed_menu = "";
        }
        if (fake_bound) UpdateFakeWV(fake_bound);
    }
    if (!next_menu_is_chest) {
        const auto fake_id = GetFakeID(chest_id);
        if (const auto container_ref = GetContainerLocation(fake_id);
            container_ref && real_bound && container_ref->GetBaseObject() == real_bound) {
            Animations::SendAnimEvent(3, container_ref);
        } else {
            Animations::SendAnimEvent(1, real_bound);
        }
    }
}

void Manager::OnChestEnter(RE::TESObjectREFR* a_chest) {
    const RefID chest_id = a_chest->GetFormID();
    queued_chests.erase(chest_id);
    if (const auto real_bound = GetRealBound(chest_id)) {
        reals_to_takeback.insert(chest_id);
        const auto unownedChestOG = RE::TESForm::LookupByID<RE::TESObjectREFR>(UnownedStuff::unownedChestOGRefID);
        RemoveItem(a_chest, unownedChestOG, real_bound, RE::ITEM_REMOVE_REASON::kStoreInContainer);
        if (const auto fake_bound = GetFakeBound(chest_id)) fake_bound->formFlags = 13;
    }
}

void Manager::HandleDrop(RE::TESObjectREFR* fake_object) {
    const auto fake_id = fake_object->GetBaseObject()->GetFormID();
    if (IsFakeContainer(fake_id)) {
        const auto chestID = GetFakeContainerChestID(fake_id);
        const auto real_bound = GetRealBound(chestID);
        WorldObject::SwapObjects(fake_object, real_bound, false);
        UpdateLoc_Private(chestID, fake_object->GetFormID());
    } else {
        logger::warn("Fake object not found in ChestToFakeContainer.");
        if (const auto baseform = DynamicFormTracker::GetSingleton()->GetOGFormOfDynamic(
            fake_object->GetBaseObject()->GetFormID())) {
            WorldObject::SwapObjects(fake_object, skyrim_cast<RE::TESBoundObject*>(baseform), false);
        }
    }
}

void Manager::BeforePickup(RE::TESObjectREFR* picked_up_by, RE::TESObjectREFR* a_object) {
    if (const auto chest = GetContainerChest(a_object)) {
        const auto chest_refid = chest->GetFormID();

        RE::TESBoundObject* fake_bound = nullptr;
        float weight_ratio = 0.f;
        {
            UNIQUE_GUARD;
            if (const auto src = GetContainerSource_NoLock(GetRealID_NoLock(chest_refid))) {
                weight_ratio = src->weight_ratio;
                if (fake_bound = GetFakeBound_NoLock(chest_refid); fake_bound && src->data.contains(chest_refid)) {
                    src->data.at(chest_refid) = picked_up_by->GetFormID();
                } else {
                    logger::critical("Fake bound not found.");
                }
            }
        }

        if (fake_bound) {
            WorldObject::SwapObjects(a_object, fake_bound, false);
            UpdateFakeWV(fake_bound, chest, weight_ratio);
            if (Settings::other_settings[Settings::otherstuffKeys[1]]) {
                auto ref_handle = picked_up_by->GetHandle();
                const auto fake_id = fake_bound->GetFormID();
                SKSE::GetTaskInterface()->AddTask([this, fake_id, ref_handle]() {
                    if (const auto ref = ref_handle.get()) {
                        RemoveCarryWeightBoost(fake_id, ref.get());
                    }
                });
            }
        }
    }
}

void Manager::OnConsume(const FormID fake_formid, RE::TESObjectREFR* consumed_by) {
    const auto fake_obj = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
    const auto chest = GetFakeContainerChest(fake_obj);
    if (!chest) return;
    const auto a_chestID = chest->GetFormID();
    const auto real_bound = FakeToRealContainer(fake_formid);
    if (!real_bound) return;
    {
        SHARED_GUARD;
        if (const auto src = GetContainerSource_NoLock(real_bound->GetFormID())) {
            if (const auto it = src->data.find(a_chestID);
                it == src->data.end() || it->second != consumed_by->GetFormID()) {
                if (!ModCompatibility::Mods::doppelgangers.contains(consumed_by->GetBaseObject()->GetFormID())) {
                    logger::error("Fake object not supposed to be in consumed_by {:x} {:x}.", consumed_by->GetFormID(),
                                  consumed_by->GetBaseObject()->GetFormID());
                }
                return;
            }
        }
    }
    chest->RemoveItem(real_bound, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
    if (!DeRegister(chest, consumed_by)) {
        RaiseMngrErr("Failed to deregister chest");
    }
    Menu::UpdateItemList();
    RE::SendUIMessage::SendInventoryUpdateMessage(player_ref, nullptr);
}

void Manager::HandleSell(const FormID a_fake, RE::TESObjectREFR* sell_ref) {
    auto chestID = GetFakeContainerChestID(a_fake);
    const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chestID);
    if (!chest) {
        RaiseMngrErr("Chest is null");
        return;
    }
    for (const auto a_child : GetChildChests(chestID, nullptr)) {
        if (const auto child_fake = GetFakeID(a_child)) HandleSell(child_fake, sell_ref);
    }
    if (!DeRegister(chest, sell_ref)) {
        RaiseMngrErr(std::format("DeRegister failed during HandleSell for chest: {:x}", chestID));
    }
}

bool Manager::IsChest(const RefID a_refid) const {
    SHARED_GUARD;
    return IsChest_NoLock(a_refid);
}

bool Manager::IsRealContainer(const FormID formid) const {
    SHARED_GUARD;
    return std::ranges::any_of(sources, [formid](const Source& src) { return src.formid == formid; });
}

bool Manager::IsFakeContainer(const FormID formid) const {
    SHARED_GUARD;
    return std::ranges::any_of(ChestToFakeContainer, [formid](const auto& pair) {
        return pair.second.innerKey == formid;
    });
}

RE::TESBoundObject* Manager::GetFakeBound(const RE::TESObjectREFR* a_loc) const {
    if (const auto chest = GetContainerChest(a_loc)) {
        return GetFakeBound(chest->GetFormID());
    }
    return nullptr;
}

void Manager::HandleFakePlacement(RE::TESObjectREFR* external_cont) {
    if (std::ranges::find(handled_external_conts, external_cont->GetFormID()) != handled_external_conts.end()) return;
    if (!external_cont->HasContainer()) return;
    if (IsUnownedChest(external_cont->GetFormID())) return;
    const auto external_cont_refid = external_cont->GetFormID();
    if (!IsARegistry(external_cont_refid)) return;
    std::vector<std::pair<RefID, RefID>> chest_locs;
    {
        SHARED_GUARD;
        for (const auto& src : sources) {
            for (const auto& [chest_ref, loc] : src.data) {
                if (external_cont_refid == loc) {
                    chest_locs.push_back({chest_ref, loc});
                }
            }
        }
    }
    for (auto& [chest_ref,loc] : chest_locs) {
        FakePlacement(loc, chest_ref, external_cont);
    }
    handled_external_conts.push_back(external_cont_refid);
}

void Manager::HandleCraftingEnter(const RefID a_furn) const {
    if (const auto a_chestID = GetContainerChestID(a_furn); a_chestID > 0) {
        const auto a_real_id = GetRealID(a_chestID);
        if (const auto src = GetContainerSource(a_real_id); src && src->transfer_on_use) {
            TransferOnUse(a_chestID);
        }
    }
}

void Manager::HandleCraftingExit() {
    for (auto& [a_chestID,a_cache] : transfer_cache) {
        if (!a_cache.empty()) {
            if (const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(a_chestID)) {
                for (const auto& [item_formid, count] : a_cache) {
                    if (const auto item = RE::TESForm::LookupByID<RE::TESBoundObject>(item_formid)) {
                        player_ref->RemoveItem(item, count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, chest);
                    }
                }
            }
        }
    }
    transfer_cache.clear();

    bool error = false;
    {
        SHARED_GUARD;
        for (const auto& src : sources) {
            if (error) {
                break;
            }
            for (const auto& [chest_refid, loc_refid] : src.data) {
                if (loc_refid != player_refid) continue;
                auto it = ChestToFakeContainer.find(chest_refid);
                if (it == ChestToFakeContainer.end()) continue;
                const auto fake_formid = it->second.innerKey;
                const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
                if (!fake_bound) {
                    logger::error("Fake bound not found for chest_refid: {:x}", chest_refid);
                    error = true;
                    break;
                }
                const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
                if (!chest) {
                    logger::error("Chest not found for chest_refid: {:x}", chest_refid);
                    error = true;
                    break;
                }
                if (Inventory::HasItem(fake_bound, player_ref)) {
                    if (!xData::UpdateExtrasInInventory(player_ref, fake_formid, chest, src.formid)) {
                        logger::error("Failed to update extras in player's inventory.");
                    }
                }
            }
        }
    }

    if (error) {
        RaiseMngrErr("Error in HandleCraftingExit");
    }
}

void Manager::HandleFormDelete(const RefID refid) {
    if (IsChest(refid)) return HandleFormDelete_(refid);
    // Find chest_ref without holding the lock during callback
    RefID targetChest = 0;
    {
        SHARED_GUARD;
        for (auto& src : sources) {
            for (const auto& [chest_ref, cont_ref] : src.data) {
                if (cont_ref == refid) {
                    targetChest = chest_ref;
                    break;
                }
            }
            if (targetChest) break;
        }
    }
    if (targetChest) HandleFormDelete_(targetChest);
}

bool Manager::IsRealContainer(const RE::TESObjectREFR* ref) const {
    if (!ref) return false;
    if (ref->IsDisabled()) return false;
    if (ref->IsDeleted()) return false;
    const auto base = ref->GetBaseObject();
    if (!base) return false;
    return IsRealContainer(base->GetFormID());
}

std::vector<Source> Manager::GetSources() const {
    SHARED_GUARD;
    return sources;
}

void Manager::Uninstall() {
    if (isUninstalled.load()) return;
    bool uninstall_successful = true;
    logger::info("Uninstalling...");
    logger::info("No of chests in cell: {}", GetNoChests());
    std::vector<std::pair<RefID, FormID>> all_chests_fakes;
    {
        SHARED_GUARD;
        for (const auto& [chest_refid, real_fake_formid] : ChestToFakeContainer) {
            all_chests_fakes.emplace_back(chest_refid, real_fake_formid.innerKey);
        }
    }
    for (const auto& chest_refid : all_chests_fakes | std::views::keys) {
        if (const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid); !chest) {
            uninstall_successful = false;
            logger::error("Chest not found");
            break;
        } else if (IsChest(chest_refid) && !DeRegister(chest, player_ref)) {
            uninstall_successful = false;
            logger::error("Failed to deregister chest during uninstall.");
        }
    }
    logger::info("Removing all unowned chests");
    {
        RE::BSSpinLockGuard locker(unownedCell->GetRuntimeData().spinLock);
        for (auto& unownedRuntimeData = unownedCell->GetRuntimeData(); const auto& ref : unownedRuntimeData.
             references) {
            if (!ref) continue;
            if (ref->GetFormID() == UnownedStuff::unownedChestOGRefID) continue;
            if (ref->GetBaseObject()->GetFormID() != UnownedStuff::unownedChestFormID) continue;
            if (ref->IsDisabled() && ref->IsDeleted()) continue;
            logger::info("Removing items from chest with refid {}", ref->GetFormID());
            if (IsChest(ref->GetFormID()) && !DeRegister(ref.get(), player_ref)) {
                uninstall_successful = false;
                logger::error("Failed to deregister chest during uninstall.");
            }
            RE::GarbageCollector::GetSingleton()->Add(ref.get(), true);
        }
    }
    logger::info("uninstall_successful: {}", uninstall_successful);
    logger::info("No of chests in cell: {}", GetNoChests());
    if (GetNoChests() != 1) uninstall_successful = false;
    logger::info("uninstall_successful: {}", uninstall_successful);
    if (uninstall_successful) {
        Reset();
        logger::info("Uninstall successful.");
        MsgBoxesNotifs::InGame::UninstallSuccessful();
    } else {
        logger::critical("Uninstall failed.");
        MsgBoxesNotifs::InGame::UninstallFailed();
    }
    DynamicFormTracker::GetSingleton()->DeleteAll();
    isUninstalled.store(true);
}

RE::TESBoundObject* Manager::FakeToRealContainer(const FormID fake) const {
    return GetRealBound(GetFakeContainerChestID(fake));
}

void Manager::OnPromptAccept(RE::TESObjectREFR* a_container, const int msgbox_action, const int a_delay) {
    if (!HandleRegistration(a_container)) return;
    if (msgbox_action == 0) {
        if (const auto chest = GetContainerChest(a_container)) queued_chests.insert(chest->GetFormID());
    }
    auto a_handle = a_container->GetHandle();
    if (a_delay > 0) {
        clib_utilsQTR::Tasker::GetSingleton()->PushTask([this,msgbox_action,a_handle] {
            Gateway(msgbox_action, a_handle);
        }, a_delay);
    } else {
        return Gateway(msgbox_action, a_handle);
    }
}

std::string Manager::GetWeightText(RE::TESObjectREFR* a_container) {
    if (const auto chest = GetContainerChest(a_container)) {
        return GetWeightText_(chest);
    }
    if (const auto src = GetContainerSource(a_container->GetBaseObject()->GetFormID())) {
        if (const auto a_capacity = src->capacity; a_capacity > 0.f) {
            return GetWeightText(0.f, a_capacity);
        }
    }
    return "";
}

std::string Manager::GetWeightText(const RE::TESBoundObject* fake_or_real) {
    if (const auto chest = GetFakeContainerChest(fake_or_real)) {
        return GetWeightText_(chest);
    }
    if (const auto src = GetContainerSource(fake_or_real->GetFormID())) {
        if (const auto a_capacity = src->capacity; a_capacity > 0.f) {
            return GetWeightText(0.f, a_capacity);
        }
    }
    return "";
}

std::string Manager::GetValueText(RE::TESObjectREFR* a_loc) {
    RE::TESBoundObject* a_real = a_loc->GetBaseObject();
    if (const auto fake = GetFakeBound(a_loc)) {
        const auto extra_cost = xData::GetXDataCostOverride(&a_loc->extraList);
        if (const auto a_value = fake->GetGoldValue() + extra_cost; a_value > 0) {
            return std::to_string(a_value);
        }
    }
    if ([[maybe_unused]] const auto src = GetContainerSource(a_real->GetFormID())) {
        if (const auto a_value = FunctionsSkyrim::GetItemValue(a_real, &a_loc->extraList); a_value > 0) {
            return std::to_string(a_value);
        }
    }
    return "";
}

RE::TESBoundObject* Manager::RegisterFromMenu(RE::InventoryEntryData* a_real_entry, RE::TESObjectREFR* a_owner) {
    a_owner = a_owner ? a_owner : player_ref;
    if (!a_real_entry) return nullptr;
    const auto a_real = a_real_entry->GetObject();
    if (!a_real) {
        logger::error("RegisterFromMenu: a_real is null");
        return nullptr;
    }
    const auto src = GetContainerSource(a_real->GetFormID());
    if (!src) {
        logger::error("No source found for real container {:x}", a_real->GetFormID());
        return nullptr;
    }
    const auto ChestObjRef = FindNotMatchedChest();
    if (!ChestObjRef) {
        logger::error("Failed to find a chest to register the container to.");
        return nullptr;
    }
    const auto ChestRefID = ChestObjRef->GetFormID();
    const auto fake_formid = CreateFakeContainer(a_real, ChestRefID, nullptr);
    const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
    if (!fake_bound) {
        RaiseMngrErr("Failed to lookup fake bound.");
        return nullptr;
    }
    RE::ExtraDataList* xList_fake = nullptr;
    if (Inventory::EntryHasXDataList(a_real_entry)) {
        xList_fake = xData::ConstructExtraDataList();
        if (!xData::UpdateExtras(a_real_entry->extraLists->front(), xList_fake)) {
            logger::error("Failed to update extras for fake object.");
        }
    }
    a_owner->AddObjectToContainer(fake_bound, xList_fake, 1, nullptr);
    a_owner->RemoveItem(a_real, 1, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, ChestObjRef);
    if (const auto initial_items_map = src->initial_items; !initial_items_map.empty()) {
        SKSE::GetTaskInterface()->AddTask([ChestRefID, initial_items_map] {
            const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(ChestRefID);
            if (!chest) return;
            for (const auto& [item, count] : initial_items_map) {
                if (count <= 0) continue;
                auto* bound = RE::TESForm::LookupByID<RE::TESBoundObject>(item);
                if (!bound) continue;
                chest->AddObjectToContainer(bound, nullptr, count, nullptr);
            }
        });
    }
    if (!Register_Sub(a_real->GetFormID(), fake_formid, ChestRefID, a_owner->GetFormID())) {
        RaiseMngrErr("Register_Sub failed in RegisterFromMenu.");
        return nullptr;
    }
    return fake_bound;
}

void Manager::RenameCallback(RE::TESBoundObject* a_fake) {
    if (!ModCompatibility::Mods::ui_extensions_installed) return;
    const char* container_name = a_fake->GetName();
    if (Papyrus::CallFunction("UIExtensions", "SetMenuPropertyString", {}, "UITextEntryMenu", "text", container_name)) {
        RE::TESForm* dummy1 = nullptr;
        RE::TESForm* dummy2 = nullptr;
        const auto smart = RE::make_smart<Papyrus::RenameCallbackFunctor>(a_fake);
        if (!CallFunction("UIExtensions", "OpenMenu", smart.get(), "UITextEntryMenu", dummy1, dummy2)) {
            logger::error("Failed to open UIExtensions menu.");
        }
    } else {
        logger::error("Failed to call UIExtensions functions.");
    }
}

void Manager::OnOpen(const RE::TESBoundObject* a_fake, const int delay) {
    auto chest = GetFakeContainerChest(a_fake);
    if (!chest) {
        logger::error("OnOpen: chest null");
        return;
    }
    queued_chests.insert(chest->GetFormID());
    if (delay > 0) {
        clib_utilsQTR::Tasker::GetSingleton()->PushTask([this,chest] {
            SKSE::GetTaskInterface()->AddUITask([this,chest] { OpenChestFromMenu(chest); });
        }, delay);
    } else {
        SKSE::GetTaskInterface()->AddUITask([this,chest] { OpenChestFromMenu(chest); });
    }
}

void Manager::CloseMenu() {
    containermenu_owner.reset();
    if (!Menu::GetContainerMenuOwner(containermenu_owner)) {
        containermenu_owner.reset();
    }
    closed_menu = Menu::CloseMenu();
}