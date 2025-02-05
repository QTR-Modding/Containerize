#include "Manager.h"
#include <ranges>

void Manager::SendReal(const FormID real_formid, RE::TESObjectREFR* chest) {
    const auto unownedChestOG = RE::TESForm::LookupByID<RE::TESObjectREFR>(unownedChestOGRefID);
    if (!unownedChestOG) return RaiseMngrErr("MsgBoxCallback unownedChestOG is null");
    if (const auto real_obj = RE::TESForm::LookupByID<RE::TESBoundObject>(real_formid); real_obj && !Inventory::HasItem(
        real_obj, unownedChestOG)){
        return RaiseMngrErr("Real container not found in unownedChestOG");
    }
    else {
        unownedChestOG->RemoveItem(real_obj,1,RE::ITEM_REMOVE_REASON::kStoreInContainer,nullptr,chest);
    }
}

RE::TESBoundObject* Manager::FakeToRealContainer(const FormID fake) {
    logger::trace("FakeToRealContainer");
    for (const auto& cont_forms : ChestToFakeContainer | std::views::values) {
        if (cont_forms.innerKey == fake) {
            return RE::TESForm::LookupByID<RE::TESBoundObject>(cont_forms.outerKey);
        }
    }
    return nullptr;
}

RefID Manager::GetRealContainerChestID(const RefID real_refid) const {
    for (const auto& src : sources) {
        for (const auto& [chest_refid, cont_refid] : src.data) {
            if (cont_refid == real_refid) return chest_refid;
        }
    }
    return 0;
}

RE::TESBoundObject* Manager::GetFakeBound(const RefID chest_id) const {
	if (ChestToFakeContainer.contains(chest_id)) {
		return RE::TESForm::LookupByID<RE::TESBoundObject>(ChestToFakeContainer.at(chest_id).innerKey);
	}
    return nullptr;
}

void Manager::HandlePickup(const RE::TESObjectREFR* picked_up_by, RE::TESObjectREFR* a_object)
{
    if (const auto chest = GetRealContainerChest(a_object)) {
	    const auto src = GetContainerSource(a_object->GetFormID());
		const auto chest_refid = chest->GetFormID();
        if (auto* fake_bound = GetFakeBound(chest_refid)) {
			const auto fake_id = fake_bound->GetFormID();
			src->data.at(chest_refid) = picked_up_by->GetFormID();
            WorldObject::SwapObjects(a_object, fake_bound,false);
            UpdateFakeWV(RE::TESForm::LookupByID<RE::TESBoundObject>(fake_id), chest, src->weight_ratio);
            if (other_settings[Settings::otherstuffKeys[1]]) RemoveCarryWeightBoost(fake_id, player_ref);
        }
		else {
			logger::critical("Fake bound not found.");
		}
	}
}

bool Manager::RealContainerHasRegistry(const FormID realcontainer_formid) const
{
    logger::trace("RealContainerHasRegistry");
    for (const auto& src : sources) {
        if (src.formid == realcontainer_formid) {
            if (!src.data.empty()) return true;
            break;
        }
	}
	return false;
}

void Manager::Activate(RE::TESObjectREFR* a_objref) {

    const auto a_obj = a_objref->GetBaseObject()->As<RE::TESObjectCONT>();
    if (!a_obj) {
        return RaiseMngrErr("Object is not a container");
    }
    a_obj->Activate(a_objref, player_ref, 0, a_obj, 1);
}

void Manager::ActivateChest(RE::TESObjectREFR* chest, const char* chest_name) {

    unownedChest->fullName = chest_name;
    Activate(chest);
}

void Manager::ActivateContainer(const FormID fakeid, const bool hide_real) {
    logger::trace("ActivateContainer 2 args");
    const auto chest_refid = GetFakeContainerChest(fakeid);
    const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
    const auto real_container_formid = FakeToRealContainer(fakeid)->GetFormID();
    const auto real_container_name = RE::TESForm::LookupByID<RE::TESBoundObject>(real_container_formid)->GetName();
    if (hide_real) {
        logger::trace("Hiding real container {} which is in chest {}", real_container_formid, chest_refid);
        if (const auto fake_form = RE::TESForm::LookupByID(fakeid); fake_form->formFlags != 13) fake_form->formFlags = 13;
        const auto real_refhandle =
            RemoveItem(chest, nullptr, real_container_formid, RE::ITEM_REMOVE_REASON::kDropping);
        if (!real_refhandle) return RaiseMngrErr("Real refhandle is null.");
        hidden_real_ref = real_refhandle.get().get();
    }
        
    logger::trace("Activating chest");
    const auto chest_rename = renames.contains(fakeid) ? renames[fakeid].c_str() : real_container_name;
    ActivateChest(chest, chest_rename);
}

void Manager::UnHideReal(const FormID fakeid) { 

    logger::trace("UnHideReal");

    if (!hidden_real_ref) return RaiseMngrErr("Hidden real ref is null");

    if (const auto fake_form = RE::TESForm::LookupByID(fakeid); fake_form->formFlags == 13) fake_form->formFlags = 9;

    const auto chest_refid = GetFakeContainerChest(fakeid);
    const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
    const auto real_formid = ChestToFakeContainer[chest_refid].outerKey;
    if (hidden_real_ref->GetBaseObject()->GetFormID() != real_formid) {
        return RaiseMngrErr("Hidden real ref formid does not match the real formid");
    }
    if (!MoveObject(hidden_real_ref, chest)) return RaiseMngrErr("Failed to UnHideReal");
    const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fakeid);
    hidden_real_ref = nullptr;
    const auto src = GetContainerSource(real_formid);
    if (!src) return RaiseMngrErr("Could not find source for container");
    UpdateFakeWV(fake_bound, chest, src->weight_ratio);
}

void Manager::DropTake(const FormID realcontainer_formid, const RefID refid)
{
    // Assumes that the real container is in the player's inventory!

    logger::info("DropTaking...");

    if (!IsRealContainer(realcontainer_formid)) {
        logger::warn("Only real containers allowed");
    }
    if (!RealContainerHasRegistry(realcontainer_formid)) {
        logger::warn("Real container has no registry");
    }
    
    const auto src = GetContainerSource(realcontainer_formid);
    const auto refhandle = RemoveItem(player_ref, nullptr, realcontainer_formid, RE::ITEM_REMOVE_REASON::kDropping);
	if (!refhandle) return RaiseMngrErr("Real refhandle is null.");
#ifndef NDEBUG
    if (refhandle.get()->GetFormID() == refid) {
        logger::trace("Native handle is the same as the refhandle refid");
    }
    else logger::trace("Native handle is NOT the same as the refhandle refid");
#endif
    const auto chest_refid = GetRealContainerChestID(refid);
    auto* fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(ChestToFakeContainer[chest_refid].innerKey);
    WorldObject::SwapObjects(refhandle.get().get(), fake_bound,false);
    if (!PickUpItem(refhandle.get().get())) {
		RaiseMngrErr("DropTake: Failed to pick up the item.");
	}
    src->data[chest_refid] = chest_refid;
}

void Manager::HandleCraftingExit() {
    logger::trace("HandleCraftingExit");

    listen_container_change.store(false);

    logger::trace("Crafting menu closed");
    for (auto& src : sources) {
        for (const auto& [chest_refid, cont_refid] : src.data) {
            // we trust that the player will leave the crafting menu at some point and everything will be reverted
            if (chest_refid != cont_refid) continue;  // playerda deilse continue
            const auto fake_formid = ChestToFakeContainer[chest_refid].innerKey;
            const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
            if (!fake_bound) return RaiseMngrErr("Fake bound not found.");
            const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
            if (!chest) return RaiseMngrErr("Chest is null");
            if (!Inventory::HasItem(fake_bound,player_ref)){
                // it can happen when using arcane enchanter to destroy the item
                logger::info("Player does not have fake item. Probably destroyed in arcane enchanter.");
                RemoveItem(chest, nullptr, src.formid, RE::ITEM_REMOVE_REASON::kRemove);
                DeRegisterChest(chest_refid);
                continue;
            }
            if (!UpdateExtrasInInventory(player_ref, fake_formid, chest, src.formid)) {
                logger::error("Failed to update extras in player's inventory.");
                return;
            }
                
            /*auto fake_refhandle = RemoveItem(player_ref, nullptr, src.formid, RE::ITEM_REMOVE_REASON::kDropping);
                auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;*/
            /*if (is_equipped[fake_formid]) EquipItem(fake_bound);
                if (is_faved[fake_formid]) FaveItem(fake_bound);*/
                
            //FetchFake(nullptr, fake_formid, chest_ref, real_refhandle.get().get()); // (< v0.7.1)
        }
    }

    listen_container_change.store(true);

}

void Manager::HandleConsume(const FormID fake_formid) {
    logger::trace("HandleConsume");
    // check if player has the fake item
    // sometimes player does not have the fake item but it can still be there with count = 0.
    const auto fake_obj = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
    if (!fake_obj) return RaiseMngrErr("Fake object not found.");
        
    // the cleanup might actually not be necessary since DeRegisterChest will remove it from player
    if (HasItemPlusCleanUp(fake_obj,player_ref)) return; // false alarm
    // ok. player does not have the fake item.
    // make sure unownedOG does not have it in inventory
    // < v0.7.1
    //if (HasItemPlusCleanUp(fake_obj, unownedChestOG)) { // false alarm???
    //    logger::warn("UnownedchestOG has item.");
    //    return;
    //}

    // make also sure that the real counterpart is still in unowned
    const auto chest_refid = GetFakeContainerChest(fake_formid);
    const auto chest_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
    const auto real_obj = FakeToRealContainer(fake_formid);
    if (!real_obj) return RaiseMngrErr("Real counterpart not found.");
    if (!Inventory::HasItem(real_obj, chest_ref)) return RaiseMngrErr("Real counterpart not found in unowned chest.");
        
    logger::info("Deregistering bcs Item consumed.");
    RemoveItem(chest_ref, nullptr, real_obj->GetFormID(), RE::ITEM_REMOVE_REASON::kRemove);
    for (const auto temp_formids = DeRegisterChest(chest_refid); auto& temp_formid : temp_formids){
        if (const auto temp_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(temp_formid)){
            RE::SendUIMessage::SendInventoryUpdateMessage(player_ref, temp_bound);
        }
    }

}

bool Manager::IsCONT(const RefID refid) {
    logger::trace("IsCONT");
    return RE::FormTypeToString(
               RE::TESForm::LookupByID<RE::TESObjectREFR>(refid)->GetObjectReference()->GetFormType()) == "CONT";
}

int Manager::GetChestValue(RE::TESObjectREFR* a_chest) {
    logger::trace("GetChestValue");
    if (!a_chest) {
        RaiseMngrErr("Chest is null");
        return 0;
    }
    const auto chest_inventory = a_chest->GetInventory();
    int total_value = 0;
    for (const auto& snd : chest_inventory | std::views::values) {
        const auto item_count = snd.first;
        const auto inv_data = snd.second.get();
        total_value += inv_data->GetValue() * item_count;
    }
    return total_value;
}

RE::TESObjectREFR* Manager::GetRealContainerChest(const RE::TESObjectREFR* real_container) {

    const RefID container_refid = real_container->GetFormID();

    if (const auto src = GetContainerSource(real_container->GetBaseObject()->GetFormID())) {
        if (const auto chest_refid = GetRealContainerChestID(container_refid)) {
            if (const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid)) {
				return chest;
            }
        }
    }

    return nullptr;
}

uint32_t Manager::GetNoChests() const {
    logger::trace("Getting number of chests");
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

std::vector<RefID> Manager::ConnectedChests(const RefID chestRef) {
    std::vector<RefID> connected_chests;
    for (const auto& [chest_ref, cont_ref] : ChestToFakeContainer) {
        const auto src = GetContainerSource(cont_ref.outerKey);
        if (!src) {
            logger::error("Source not found for formid: {:x}", cont_ref.outerKey);
            continue;
        }
        if (chestRef != chest_ref && src->data[chest_ref] == chestRef) {
            connected_chests.push_back(chest_ref);
        }
    }
    return connected_chests;
}

bool Manager::IsUnownedChest(const RefID refid) const {
    logger::trace("IsUnownedChest");
    const auto* temp = RE::TESForm::LookupByID<RE::TESObjectREFR>(refid);
    if (!temp) return false;
    const auto base = temp->GetBaseObject();
    return base ? base->GetFormID() == unownedChest->GetFormID() : false;
}

RE::TESObjectREFR* Manager::MakeChest(const RE::NiPoint3 Pos3) const {
    logger::info("Making chest");
    const auto item = unownedChest->As<RE::TESBoundObject>();

    const auto newPropRef = RE::TESDataHandler::GetSingleton()
                            ->CreateReferenceAtLocation(item, Pos3, {0.0f, 0.0f, 0.0f}, unownedCell, nullptr, nullptr,
                                                        nullptr, {}, true, false).get().get();
    logger::info("Created Object! Type: {}, Base ID: {:x}, Ref ID: {:x},",
                 RE::FormTypeToString(item->GetFormType()), item->GetFormID(), newPropRef->GetFormID());
    return newPropRef;
}

RE::TESObjectREFR* Manager::AddChest(const uint32_t chest_no) const {
    logger::trace("Adding chest");
    int total_chests = static_cast<int>(chest_no);
    total_chests += 1;
    /*int total_chests_x = (((total_chests - 1) + 2) % 5) - 2;
        int total_chests_y = ((total_chests - 1) / 5) % 9;
        int total_chests_z = (total_chests - 1) / 45;*/
    const int total_chests_x = (1 - (total_chests % 3)) * (-2);
    const int total_chests_y = ((total_chests - 1) / 3) % 9;
    const int total_chests_z = (total_chests - 1) / 27;
    const float Pos3_x = unownedChestPos.x + static_cast<float>(100 * total_chests_x);
    const float Pos3_y = unownedChestPos.y + static_cast<float>(50 * total_chests_y);
    const float Pos3_z = unownedChestPos.z + static_cast<float>(50 * total_chests_z);
    const RE::NiPoint3 Pos3 = {Pos3_x, Pos3_y, Pos3_z};
    return MakeChest(Pos3);
}

RE::TESObjectREFR* Manager::FindNotMatchedChest() const {
    logger::trace("Finding not matched chest");

    auto& runtimeData = unownedCell->GetRuntimeData();
    RE::BSSpinLockGuard locker(runtimeData.spinLock);
    for (const auto& ref : runtimeData.references) {
        if (!ref) continue;
        if (ref->GetFormID() == unownedChestOGRefID) continue;
		if (ref->GetBaseObject()->GetFormID() != unownedChest->GetFormID()) continue;
        const size_t n_items = ref->GetInventory().size();
        bool contains_key = false;
        for (const auto& src : sources) {
            if (src.data.contains(ref->GetFormID())) {
                contains_key = true;
                break;
            }
        }
        if (!n_items && !contains_key) {
            return ref.get();
        }
    }
    return AddChest(GetNoChests());
}

RefID Manager::GetFakeContainerChest(const FormID fake_id) {
    logger::trace("GetFakeContainerChest");
    if (!fake_id) {
        RaiseMngrErr("Fake container refid is null");
        return 0;
    }
    for (const auto& [chest_ref, cont_forms] : ChestToFakeContainer) {
        if (cont_forms.innerKey == fake_id) return chest_ref;
    }
    RaiseMngrErr("Fake container refid not found in ChestToFakeContainer");
    return 0;
}

std::vector<FormID> Manager::RemoveAllItemsFromChest(RE::TESObjectREFR* chest, RE::TESObjectREFR* move2ref) {

    logger::trace("RemoveAllItemsFromChest");

    std::vector<FormID> removed_objects;

    if (!chest) {
        RaiseMngrErr("Chest is null");
        return removed_objects;
    }

    listen_container_change.store(false);

    const auto chest_container = chest->GetContainer();
    if (!chest_container) {
        logger::error("Chest container is null");
        MsgBoxesNotifs::InGame::GeneralErr();
        return removed_objects;
    }

    if (move2ref && !move2ref->HasContainer()){
        logger::error("move2ref has no container");
        move2ref = nullptr;
    }

    auto removeReason = move2ref ? RE::ITEM_REMOVE_REASON::kStoreInContainer : RE::ITEM_REMOVE_REASON::kRemove;
    if (move2ref && move2ref->IsPlayerRef()) removeReason = RE::ITEM_REMOVE_REASON::kRemove;

    for (const auto inventory = chest->GetInventory(); const auto& item : inventory) {
        const auto item_obj = item.first;
        if (!std::strlen(item_obj->GetName())) logger::warn("RemoveAllItemsFromChest: Item name is empty");
        auto item_count = item.second.first;
        const auto inv_data = item.second.second.get();
        if (const auto asd = inv_data->extraLists; !asd || asd->empty()) {
            logger::trace("Removing item: {} with count: {} and remove reason", item_obj->GetName(), item_count);
            chest->RemoveItem(item_obj, item_count, removeReason, nullptr, move2ref);
        } else {
            logger::trace("Removing item with extradata: {} with count: {}", item_obj->GetName(), item_count);
            chest->RemoveItem(item_obj, item_count, removeReason, asd->front(), move2ref);
        }
        removed_objects.push_back(item_obj->GetFormID());
    }
        
    logger::trace("Checking for fake containers in chest");
    // need to handle if a fake container was inside this chest. yani cont_ref i bu cheste bakan data varsa
    // redirectlemeliyim
    const auto chest_refid = chest->GetFormID();
    for (auto& src : sources) {
        if (!Functions::containsValue(src.data, chest_refid)) continue;
        if (!move2ref) RaiseMngrErr("move2ref is null, but a fake container was found in the chest.");
        for (const auto& [key, value] : src.data) {
            if (value == chest_refid && key != value) {
                logger::info(
                    "Fake container with formid {:x} found in chest during RemoveAllItemsFromChest. Redirecting...",
                    ChestToFakeContainer[key].innerKey);
                if (move2ref->IsPlayerRef()) src.data[key] = key;
                // must be sell_ref
                else {
                    // the chest that is connected to the fake container which was inside this chest
                    HandleSell(ChestToFakeContainer[key].innerKey, move2ref->GetFormID());
                    listen_container_change.store(true);
                }
            }
        }
    }

    listen_container_change.store(true);
        
    return removed_objects;
}

std::vector<FormID> Manager::DeRegisterChest(const RefID chest_ref) {
    logger::info("Deregistering chest");
    std::vector<FormID> removed_stuff;
    const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref);
    if (!chest) {
        RaiseMngrErr("Chest not found");
        return removed_stuff;
    }
    const auto src = GetContainerSource(ChestToFakeContainer[chest_ref].outerKey);
    if (!src) {
        RaiseMngrErr("Source not found");
        return removed_stuff;
    }
    removed_stuff = RemoveAllItemsFromChest(chest, player_ref);
    // also remove the associated fake item from player or unowned chest// (<0.7.1)
    //auto fake_id = ChestToFakeContainer[chest_ref].innerKey;
    //if (fake_id) {
    //    RemoveItem(player_ref, nullptr, fake_id, RE::ITEM_REMOVE_REASON::kRemove);
    //    RemoveItem(unownedChestOG, nullptr, fake_id, RE::ITEM_REMOVE_REASON::kRemove); 
    //}
    if (!src->data.erase(chest_ref)) {
        RaiseMngrErr("Failed to remove chest refid from source");
        return removed_stuff;
    }
    if (!ChestToFakeContainer.erase(chest_ref)) {
        RaiseMngrErr("Failed to erase chest refid from ChestToFakeContainer");
        return removed_stuff;
    }
    // make sure no item is left in the chest
    if (!chest->GetInventory().empty()) {
        RaiseMngrErr("Chest still has items in it. Degistering failed");
        return removed_stuff;
    }   

    return removed_stuff;
}

Source* Manager::GetContainerSource(const FormID container_formid) {
    logger::trace("GetContainerSource");
    for (auto& src : sources) {
        if (src.formid == container_formid) {
            return &src;
        }
    }
    logger::error("Container source not found");
    return nullptr;
}

bool Manager::HasItemPlusCleanUp(RE::TESBoundObject* item, RE::TESObjectREFR* item_owner) {
    logger::trace("HasItemPlusCleanUp");
    const auto inventory = item_owner->GetInventory();
    if (const auto entry = inventory.find(item); entry == inventory.end()) return false;
    else if (entry->second.first > 0) return true;
    RemoveItem(item_owner, nullptr, item->GetFormID(), RE::ITEM_REMOVE_REASON::kRemove);
    return false;
}

void Manager::Uninstall() {

	if (isUninstalled.load()) return;

    bool uninstall_successful = true;

    logger::info("Uninstalling...");
    logger::info("No of chests in cell: {}", GetNoChests());

    // first lets get rid of the fake items from everywhere
    for (const auto& [chest_refid, real_fake_formid] : ChestToFakeContainer) {
        const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
        if (!chest) {
            uninstall_successful = false;
            logger::error("Chest not found");
            break;
        }
        const auto fake_formid = real_fake_formid.innerKey;
        const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
        if (!fake_bound) {
            uninstall_successful = false;
            logger::error("Fake bound not found");
            break;
        }
        int max_try = 10;
        while (HasItemPlusCleanUp(fake_bound, chest) && max_try>0) {
            RemoveItem(chest, nullptr, fake_formid, RE::ITEM_REMOVE_REASON::kRemove);
            max_try--;
        }
        max_try = 10;
        while (HasItemPlusCleanUp(fake_bound, player_ref) && max_try>0) {
            RemoveItem(player_ref, nullptr, fake_formid, RE::ITEM_REMOVE_REASON::kRemove);
            max_try--;
        }
    }

    // Delete all unowned chests and try to return all items to the player's inventory while doing that
    for (auto& unownedRuntimeData = unownedCell->GetRuntimeData(); const auto& ref : unownedRuntimeData.references) {
        if (!ref) continue;
        if (ref->GetFormID() == unownedChestOGRefID) continue;
        if (ref->GetBaseObject()->GetFormID() != unownedChestFormID) continue;
        if (ref->IsDisabled() && ref->IsDeleted()) continue;
        logger::info("Removing items from chest with refid {}", ref->GetFormID());
        RemoveAllItemsFromChest(ref.get(), player_ref);
        ref->Disable();
        ref->SetDelete(true);
    }

    // seems like i need to do it for the player again???????
    for (const auto& [chest_refid, real_fake_formid] : ChestToFakeContainer) {
        if (const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid); !chest) {
            uninstall_successful = false;
            logger::error("Chest not found");
            break;
        }
        const auto fake_formid = real_fake_formid.innerKey;
        auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
        if (!fake_bound) {
            uninstall_successful = false;
            logger::error("Fake bound not found");
            break;
        }
        if (player_ref->GetInventory().contains(fake_bound)) {
            RemoveItem(player_ref, nullptr, fake_formid, RE::ITEM_REMOVE_REASON::kRemove);
        }
    }


    Reset();

    logger::info("uninstall_successful: {}", uninstall_successful);

    logger::info("No of chests in cell: {}", GetNoChests());

    if (GetNoChests() != 1) uninstall_successful = false;

    logger::info("uninstall_successful: {}", uninstall_successful);

    if (uninstall_successful) {
        // sources.clear();
        /*current_container = nullptr;
            unownedChestOG = nullptr;*/
        logger::info("Uninstall successful.");
        MsgBoxesNotifs::InGame::UninstallSuccessful();
    } else {
        logger::critical("Uninstall failed.");
        MsgBoxesNotifs::InGame::UninstallFailed();
    }

    listen_activate.store(true); // this one stays
    listen_container_change.store(false);
    // set uninstalled flag to true
    isUninstalled.store(true);
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

void Manager::Init() {

    bool init_failed = false;

    if (sources.empty()) {
        logger::error("No sources found.");
        InitFailed();
        return;
    }

    // Check for invalid sources (form types etc.)
    for (auto& src : sources) {
        const auto form_ = GetFormByID(src.formid,src.editorid);
        if (const auto bound_ = src.GetBoundObject(); !form_ || !bound_) {
            init_failed = true;
            logger::error("Failed to initialize Manager due to missing source: {}, {}", src.formid, src.editorid);
            break;
        }
        auto formtype_ = RE::FormTypeToString(form_->GetFormType());
        if (std::string formtypeString(formtype_); !Settings::AllowedFormTypes.contains(formtypeString)) {
            init_failed = true;
            MsgBoxesNotifs::InGame::FormTypeErr(form_->formID);
            logger::error("Failed to initialize Manager due to invalid source type: {}",formtype_);
            break;
        }
    }

    // Check for duplicate formids
    std::unordered_set<std::uint32_t> encounteredFormIDs;
    for (const auto& source : sources) {
        // Check if the formid is already encountered
        if (!encounteredFormIDs.insert(source.formid).second) {
            // Duplicate formid found
            logger::error("Duplicate formid found: {}", source.formid);
            init_failed = true;
        }
    }

    logger::info("No of chests in cell: {}", GetNoChests());
    const auto unownedChestOG = RE::TESForm::LookupByID<RE::TESObjectREFR>(0x000EA29A);
    if (!unownedChestOG || unownedChestOG->GetBaseObject()->GetFormID() != unownedChest->GetFormID() ||
        !unownedCell ||
        !unownedChest ||
        !unownedChest->As<RE::TESBoundObject>()) {
        logger::error("Failed to initialize Manager due to missing unowned chest/cell");
        init_failed = true;
    }

    if (Settings::is_pre_0_7_1) RemoveAllItemsFromChest(unownedChestOG);

    if (init_failed) return InitFailed();

    // Load also other settings...

    //if (other_settings[Settings::otherstuffKeys[1]]) {
        /*empty_mgeff = RE::IFormFactory::GetConcreteFormFactoryByType<RE::EffectSetting>()->Create();
            if (!empty_mgeff) {
                logger::critical("Failed to create empty mgeff.");
                init_failed = true;
            } else {
                empty_mgeff->magicItemDescription = std::string(" ");
                empty_mgeff->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kNoDuration);
            }*/
    //}

    const auto data_handler = RE::TESDataHandler::GetSingleton();
    if (!data_handler) return RaiseMngrErr("Data handler is null");
    if (!data_handler->LookupModByName("UIExtensions.esp")) uiextensions_is_present = false;
    else uiextensions_is_present = true;
        

    logger::info("Manager initialized.");
}

void Manager::HandleSell(const FormID fake_container, const RefID sell_refid) {
    // assumes the sell_refid is a container

    logger::trace("HandleSell");

    const auto sell_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(sell_refid);
    logger::trace("sell ref name: {}", sell_ref->GetName());
    if (!sell_ref) return RaiseMngrErr("Sell ref not found");
    // remove the fake container from vendor
    logger::trace("Removed fake container from vendor");
    // add the real container to the vendor from the unownedchest
    const auto chest_refid = GetFakeContainerChest(fake_container);
    const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
    if (!chest) return RaiseMngrErr("Chest not found");
    RemoveItem(sell_ref, nullptr, fake_container, RE::ITEM_REMOVE_REASON::kRemove);
    if (other_settings[Settings::otherstuffKeys[3]]) RemoveAllItemsFromChest(chest, sell_ref);
    else RemoveItem(chest, sell_ref, ChestToFakeContainer[chest_refid].outerKey,
                           RE::ITEM_REMOVE_REASON::kStoreInContainer);
    logger::trace("Added real container to vendor chest");
    // remove all items from the chest to the player's inventory and deregister this chest
    DeRegisterChest(chest_refid);
    logger::trace("Sell handled.");
}

void Manager::HandleFormDelete(const RefID refid) {
    //std::lock_guard<std::mutex> lock(mutex);
    if (ChestToFakeContainer.contains(refid)) return HandleFormDelete(refid);
    // the deleted reference could also be a real container out in the world.
    // in that case i need to return the items from its chest
    for (auto& src : sources) {
        for (const auto& [chest_ref, cont_ref] : src.data) {
            if (cont_ref == refid) {
                logger::warn("Form with refid {} is deleted. Removing it from the manager.", chest_ref);
                return HandleFormDelete_(chest_ref);
            }
        }
    }
}

FormID Manager::CreateFakeContainer(RE::TESBoundObject* container, const RefID connected_chest, RE::ExtraDataList* el) {
    logger::trace("pre-CreateFakeContainer");
    std::string formtype(RE::FormTypeToString(container->GetFormType()));
    if (formtype == "SCRL") {return CreateFakeContainer<RE::ScrollItem>(container->As<RE::ScrollItem>(), connected_chest, el);} 
    if (formtype == "ARMO") {return CreateFakeContainer<RE::TESObjectARMO>(container->As<RE::TESObjectARMO>(), connected_chest, el);}
    if (formtype == "BOOK") {return CreateFakeContainer<RE::TESObjectBOOK>(container->As<RE::TESObjectBOOK>(), connected_chest, el);}
    if (formtype == "INGR") {return CreateFakeContainer<RE::IngredientItem>(container->As<RE::IngredientItem>(),connected_chest, el);}
    if (formtype == "MISC") {return CreateFakeContainer<RE::TESObjectMISC>(container->As<RE::TESObjectMISC>(), connected_chest, el);}
    if (formtype == "WEAP") {return CreateFakeContainer<RE::TESObjectWEAP>(container->As<RE::TESObjectWEAP>(), connected_chest, el);}
    //if (formtype == "AMMO") {return CreateFakeContainer<RE::TESAmmo>(container->As<RE::TESAmmo>(), extralist);}
    if (formtype == "SLGM") {return CreateFakeContainer<RE::TESSoulGem>(container->As<RE::TESSoulGem>(), connected_chest, el);} 
    if (formtype == "ALCH") {return CreateFakeContainer<RE::AlchemyItem>(container->As<RE::AlchemyItem>(), connected_chest, el);}
    logger::error("Form type not supported: {}", formtype);
    return 0;
}

bool Manager::IsRealContainer(const FormID formid) const {
    logger::trace("IsRealContainer");
	return std::ranges::any_of(sources, [formid](const Source& src) { return src.formid == formid; });
}

void Manager::OnActivateContainer(RE::TESObjectREFR* a_container) {
    logger::trace("OnActivateContainer 1 arg");
	if (!HandleRegistration(a_container)) return;
        
    // store it temporarily in unownedChestOG
    if (const auto unownedChestOG = RE::TESForm::LookupByID<RE::TESObjectREFR>(unownedChestOGRefID);
        !unownedChestOG) {
        return RaiseMngrErr("OnActivateContainer: unownedChestOG is null");
    }
    else if (const auto chest = GetRealContainerChest(current_container); !chest) {
        return RaiseMngrErr("OnActivateContainer: Chest not found");
    }
    else {
        const auto real_formid = ChestToFakeContainer[chest->GetFormID()].outerKey;
        RemoveItem(chest, unownedChestOG, real_formid, RE::ITEM_REMOVE_REASON::kStoreInContainer);
        return PromptInterface();
    }
}

void Manager::HandleFakePlacement(RE::TESObjectREFR* external_cont) {
    logger::trace("HandleFakePlacement");
    // if the external container is already handled (handled_external_conts) return
    if (const auto it = std::find(handled_external_conts.begin(), handled_external_conts.end(), external_cont->GetFormID()); it != handled_external_conts.end()) return;
        
    if (!external_cont) return RaiseMngrErr("external_cont is null");
    if (IsRealContainer(external_cont)) return;
    if (!external_cont->HasContainer()) return;
    if (!ExternalContainerIsRegistered(external_cont->GetFormID())) return;
    if (IsUnownedChest(external_cont->GetFormID())) return;

    listen_container_change.store(false);

    if (!external_cont) return RaiseMngrErr("external_cont is null");
    for (auto& src : sources) {
        if (!Functions::containsValue(src.data, external_cont->GetFormID())) continue;
        for (const auto& [chest_ref, cont_ref] : src.data) {
            if (external_cont->GetFormID() != cont_ref) continue;
            FakePlacement(src.data[chest_ref], chest_ref, external_cont);
            // break yok cunku baska fakeler de external_cont un icinde olabilir
        }
    }
    handled_external_conts.push_back(external_cont->GetFormID());

    listen_container_change.store(true);
}

bool Manager::IsFakeContainer(const FormID formid) {
    if (!formid) return false;
    for (const auto& cont_form : ChestToFakeContainer | std::views::values) {
        if (cont_form.innerKey == formid) return true;
    }
    return false;
}

bool Manager::IsRealContainer(const RE::TESObjectREFR* ref) const {
    logger::trace("IsRealContainer2");
    if (!ref) return false;
    if (ref->IsDisabled()) return false;
    if (ref->IsDeleted()) return false;
    const auto base = ref->GetBaseObject();
    if (!base) return false;
    return IsRealContainer(base->GetFormID());
}

void Manager::RenameContainer(const std::string& new_name) {

    logger::trace("RenameContainer");
    if (!current_container) {
		logger::trace("Current container is null");
		return;
    }
    const auto chest = GetRealContainerChest(current_container);
    if (!chest) return RaiseMngrErr("Chest not found");
    const auto fake_formid = ChestToFakeContainer[chest->GetFormID()].innerKey;
    const auto fake_form = RE::TESForm::LookupByID(fake_formid);
    if (!fake_form) return RaiseMngrErr("Fake form not found");
    const std::string formtype(RE::FormTypeToString(fake_form->GetFormType()));
    if (formtype == "SCRL") Rename(new_name, fake_form->As<RE::ScrollItem>());
    else if (formtype == "ARMO") Rename(new_name, fake_form->As<RE::TESObjectARMO>());
    else if (formtype == "BOOK") Rename(new_name, fake_form->As<RE::TESObjectBOOK>());
    else if (formtype == "INGR") Rename(new_name, fake_form->As<RE::IngredientItem>());
    else if (formtype == "MISC") Rename(new_name, fake_form->As<RE::TESObjectMISC>());
    else if (formtype == "WEAP") Rename(new_name, fake_form->As<RE::TESObjectWEAP>());
    else if (formtype == "AMMO") Rename(new_name, fake_form->As<RE::TESAmmo>());
    else if (formtype == "SLGM") Rename(new_name, fake_form->As<RE::TESSoulGem>());
    else if (formtype == "ALCH") Rename(new_name, fake_form->As<RE::AlchemyItem>());
    else logger::warn("Form type not supported: {}", formtype);

    renames[fake_formid] = new_name;
    logger::trace("Renamed fake container.");
    if (current_container){
        logger::trace("Renaming current container.");
        xData::AddTextDisplayData(&current_container->extraList, new_name);
    }

    // if reopeninitialmenu is true, then PromptInterface
    if (other_settings[Settings::otherstuffKeys[2]]) PromptInterface();
}

void Manager::RevertEquip(const FormID fakeid) {
    logger::trace("RE::TESForm::LookupByID");
    const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fakeid);
    logger::trace("RE::TESForm::LookupByID___");
    if (Inventory::IsEquipped(fake_bound)) {
        Inventory::EquipItem(fake_bound, true);
    } else {
        Inventory::EquipItem(fake_bound);
    }
}

void Manager::RevertEquip(const FormID fakeid, const RefID external_container_id) {
    logger::trace("RevertEquip");
    const auto external_container = RE::TESForm::LookupByID<RE::TESObjectREFR>(external_container_id);
    if (!external_container) return RaiseMngrErr("External container not found");
    RemoveItem(player_ref, external_container, fakeid, RE::ITEM_REMOVE_REASON::kRemove);
    LinkExternalContainer(fakeid, external_container_id);
}

void Manager::HandleContainerMenuExit() { 
    if (real_to_sendback.first && real_to_sendback.second) {
        const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(real_to_sendback.second);
        if (!chest) return RaiseMngrErr("HandleContainerMenuExit: Chest is null");
        SendReal(real_to_sendback.first, chest);
        real_to_sendback = {0,0};
    }
}

bool Manager::IsARegistry(const RefID registry) const {
    logger::trace("IsARegistry");
    for (const auto& src : sources) {
        for (const auto& cont_ref : src.data | std::views::values) {
            if (cont_ref == registry) return true;
        }
    }
    return false;
}

bool Manager::ExternalContainerIsRegistered(const RefID external_container_id) {
    logger::trace("ExternalContainerIsRegistered");
    if (!external_container_id) return false;
    if (!RE::TESForm::LookupByID<RE::TESObjectREFR>(external_container_id)->HasContainer()) {
        RaiseMngrErr("External container does not have a container.");
        return false;
    }
    return IsARegistry(external_container_id);
}

bool Manager::ExternalContainerIsRegistered(const FormID fake_container_formid, const RefID external_container_id) {
    logger::trace("ExternalContainerIsRegistered2");
    if (!external_container_id) return false;
    if (!RE::TESForm::LookupByID<RE::TESObjectREFR>(external_container_id)) return false;
    if (!RE::TESForm::LookupByID<RE::TESObjectREFR>(external_container_id)->HasContainer()) {
        RaiseMngrErr("External container does not have a container.");
        return false;
    }
    const auto real_container_formid = FakeToRealContainer(fake_container_formid)->GetFormID();
    const auto src = GetContainerSource(real_container_formid);
    if (!src) return false;
    return Functions::containsValue(src->data,external_container_id);
}

void Manager::UnLinkExternalContainer(const FormID fake_container_formid, const RefID externalcontainer) { 

    logger::trace("UnLinkExternalContainer");

    if (!RE::TESForm::LookupByID<RE::TESObjectREFR>(externalcontainer)->HasContainer()) {
        return RaiseMngrErr("External container does not have a container.");
    }

    const auto real_container_formid = FakeToRealContainer(fake_container_formid)->GetFormID();
    const auto src = GetContainerSource(real_container_formid);
    if (!src) return RaiseMngrErr("Source not found.");
    const auto chest_refid = GetFakeContainerChest(fake_container_formid);

    src->data[chest_refid] = chest_refid;

    // remove it from handled_external_conts
    //handled_external_conts.erase(externalcontainer_refid);

    // remove it from external_favs
    if (const auto it = std::ranges::find(external_favs, fake_container_formid); it != external_favs.end()) external_favs.erase(it);

    logger::trace("Unlinked external container.");
}

bool Manager::SwapDroppedFakeContainer(RE::TESObjectREFR* ref_fake) {

    logger::trace("SwapDroppedFakeContainer");

    if (!ref_fake) {
        logger::trace("Fake container is null.");
        return false;
    }

    const auto dropped_refid = ref_fake->GetFormID();
    const auto dropped_formid = ref_fake->GetBaseObject()->GetFormID();

    const auto real_base = FakeToRealContainer(dropped_formid);
    if (!real_base) {
        logger::info("real base is null");
        return false;
    }

    /*if (auto* player_cell = RE::PlayerCharacter::GetSingleton()->GetParentCell()) {
			logger::trace("Adding persistent cell xdata to ref1");
            RE::ExtraPersistentCell* xPersistentCell = RE::BSExtraData::Create<RE::ExtraPersistentCell>();
            xPersistentCell->persistentCell = player_cell;
            logger::trace("Adding persistent cell xdata to ref2");
            ref_fake->extraList.SetExtraFlags(xPersistentCell);
            logger::trace("Added persistent cell xdata to ref.");
		}*/

    ref_fake->extraList.SetOwner(RE::TESForm::LookupByID(0x07));
    if (renames.contains(dropped_formid) && !renames.at(dropped_formid).empty()) {
        xData::AddTextDisplayData(&ref_fake->extraList, renames.at(dropped_formid));
    }

    WorldObject::SwapObjects(ref_fake, real_base);
    // TODO: Sor bunu
    //if (ref_fake->GetAllowPromoteToPersistent()) ref_fake->inGameFormFlags.set(RE::TESObjectREFR::InGameFormFlag::kForcedPersistent);

    const auto src = GetContainerSource(real_base->GetFormID());
    if (!src) {
        RaiseMngrErr("Could not find source for container");
        return false;
    }

    const auto chest_refid = GetFakeContainerChest(dropped_formid);
    // update source data
    src->data[chest_refid] = dropped_refid;

    logger::trace("Swapped real container has refid if it didnt change in the process: {}", dropped_refid);
    return true;
}

void Manager::qTRICK_(const SourceDataKey chest_ref, const SourceDataVal cont_ref, const bool fake_nonexistent) {
        
    logger::trace("qTrick before execute_trick");
    const auto real_formid = ChestToFakeContainer[chest_ref].outerKey;
    const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref);
    const auto chest_cont_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(cont_ref);
    const auto real_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(real_formid);

    if (!chest) return RaiseMngrErr("Chest not found");
    if (!chest_cont_ref) return RaiseMngrErr("Container chest not found");
    if (!real_bound) return RaiseMngrErr("Real bound not found");
        
    // fake form nonexistent ise

    if (fake_nonexistent) {
        logger::trace("Executing trick");
        logger::trace("TRICK");

        const auto old_fakeid = ChestToFakeContainer[chest_ref].innerKey;  // for external_favs
        /*auto fakeid = DFT->Fetch(real_formid, real_editorid, chest_ref);
            if (!fakeid) fakeid = CreateFakeContainer(real_bound, chest_ref, nullptr);
            else DFT->EditCustomID(fakeid, chest_ref);*/
        const auto fakeid = CreateFakeContainer(real_bound, chest_ref, nullptr);
        // load game den dolayi
        logger::trace("ChestToFakeContainer (chest refid: {}) before: {}", chest_ref,
                      ChestToFakeContainer[chest_ref].innerKey);
        ChestToFakeContainer[chest_ref].innerKey = fakeid;
        logger::trace("ChestToFakeContainer (chest refid: {}) after: {}", chest_ref,
                      ChestToFakeContainer[chest_ref].innerKey);

        // if old_fakeid is in external_favs, we need to update it with new fakeid
        if (const auto it = std::ranges::find(external_favs, old_fakeid); it != external_favs.end()) {
            external_favs.erase(it);
            external_favs.push_back(ChestToFakeContainer[chest_ref].innerKey);
        }
        // same goes for renames
        if (renames.contains(old_fakeid) && ChestToFakeContainer[chest_ref].innerKey != old_fakeid) {
            renames[ChestToFakeContainer[chest_ref].innerKey] = renames[old_fakeid];
            renames.erase(old_fakeid);
        }
    }

    logger::trace("qTrick after fake_nonexistent");
    const auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;  // the new one (fake_nonexistent)

    logger::trace("FetchFake");

    const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
    if (!fake_bound) return RaiseMngrErr("Fake bound not found");

    if (fake_nonexistent) {
        RE::TESObjectREFR* fake_ref = WorldObject::DropObjectIntoTheWorld(fake_bound);
        if (!fake_ref) return RaiseMngrErr("Fake ref is null.");
        if (!PickUpItem(fake_ref)) return RaiseMngrErr("Failed to pick up fake container");
    }

    // Updates
    const auto to_inv = fake_nonexistent ? player_ref : chest_cont_ref;
    if (!UpdateExtrasInInventory(chest, real_formid, to_inv, fake_formid)) {
        logger::error("Failed to update extras");
    }

    logger::trace("Updating FakeWV");
    const auto src = GetContainerSource(real_formid);
    if (!src) return RaiseMngrErr("Could not find source for container");
    UpdateFakeWV(fake_bound, chest, src->weight_ratio);

    // fave it if it is in external_favs
    logger::trace("Fave");
    if (const auto it = std::ranges::find(external_favs, fake_formid); it != external_favs.end()) {
        logger::trace("Faving");
        Inventory::FavoriteItem(RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid), to_inv);
    }
    // Remove carry weight boost if it has
    if (other_settings[Settings::otherstuffKeys[1]]) RemoveCarryWeightBoost(fake_formid, to_inv);
}

void Manager::Something2(const RefID chest_ref, std::vector<RefID>& ha) {

    // ha: handled already
    if (std::ranges::find(ha, chest_ref) != ha.end()) return;
    logger::info("-------------------chest_ref: {} -------------------", chest_ref);
    for (const auto connected_chests = ConnectedChests(chest_ref); const auto& connected_chest : connected_chests) {
        logger::info("Connected chest: {}", connected_chest);
        Something2(connected_chest,ha);
        //ha.push_back(connected_chest);
    }
    const auto src = GetContainerSource(ChestToFakeContainer[chest_ref].outerKey);
    if (!src) return RaiseMngrErr("Could not find source for container");
    FakePlacement(src->data[chest_ref], chest_ref);
    ha.push_back(chest_ref);
    logger::info("-------------------chest_ref: {} DONE -------------------", chest_ref);
        
}

void Manager::FakePlacement(const RefID saved_ref, const RefID chest_ref, RE::TESObjectREFR* external_cont) {

    logger::trace("FakePlacement");

    // bu sadece load sirasinda
    // ya playerda olcak ya da unownedlardan birinde (containerception)
    // bu ikisi disindaki seylere load_game safhasinda bisey yapamiyorum external_cont nullptr sa
    if (!external_cont && chest_ref != saved_ref && !IsChest(saved_ref)) return;

    // saved_ref should not be realcontainer out in the world!
    if (IsRealContainer(external_cont)) {
        logger::critical("saved_ref should not be realcontainer out in the world!");
        return;
    }

    // external cont mu yoksa ya playerda ya da unownedlardan birinde miyi anliyoruz
    const RefID cont_ref = chest_ref == saved_ref ? 0x14 : saved_ref; // only changing in the case of indication of player has fake container
    const auto cont_of_fakecont = external_cont ? external_cont : RE::TESForm::LookupByID<RE::TESObjectREFR>(cont_ref);
    if (!cont_of_fakecont) return RaiseMngrErr("cont_of_fakecont not found");

    auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;  // dont use this again bcs it can change after qTRICK_
    RE::TESBoundObject* fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);

    if (fake_bound && fake_bound->IsDeleted()) {
        logger::warn("Fake container with formid {:x} is deleted. Removing it from inventory/external_container...",
                     fake_formid);
        RemoveItem(cont_of_fakecont, nullptr, fake_formid, RE::ITEM_REMOVE_REASON::kRemove);
    }

    if (!fake_bound || !HasItemPlusCleanUp(fake_bound, cont_of_fakecont)) {
        qTRICK_(chest_ref, cont_ref, true);
    }
    else {
        if (!std::strlen(fake_bound->GetName())) {
            logger::warn("Fake container found in {} with empty name.", cont_of_fakecont->GetDisplayFullName());
        }
        // her ihtimale karsi datayi yeniliyorum
        //const auto real_formid = ChestToFakeContainer[chest_ref].outerKey;
        //fake_bound->Copy(RE::TESForm::LookupByID<RE::TESForm>(real_formid));
        if (fake_formid != fake_bound->GetFormID()) {
            logger::warn("Fake container formid changed from {} to {}", fake_formid, fake_bound->GetFormID());
        }
        logger::trace("Fake container found in {} with name {} and formid {:x}.",
                      cont_of_fakecont->GetDisplayFullName(), fake_bound->GetName(), fake_bound->GetFormID());
        qTRICK_(chest_ref, cont_ref);
    }
        
    // yani playerda deilse
    if (chest_ref != saved_ref) {
        RemoveItem(player_ref, cont_of_fakecont, ChestToFakeContainer[chest_ref].innerKey,
                          RE::ITEM_REMOVE_REASON::kStoreInContainer);
    }
}

void Manager::RemoveCarryWeightBoost(const FormID item_formid, RE::TESObjectREFR* inventory_owner) {
    logger::trace("RemoveCarryWeightBoost");

    const auto item_obj = RE::TESForm::LookupByID<RE::TESBoundObject>(item_formid);
    if (!item_obj) return RaiseMngrErr("Item not found");
    if (!Inventory::HasItem(item_obj, inventory_owner)) {
        logger::warn("Item not found in player's inventory");
        return;
    }
    const auto inventory = inventory_owner->GetInventory();
    if (const auto enchantment = inventory.find(item_obj)->second.second->GetEnchantment()) {
        logger::trace("Enchantment: {}", enchantment->GetName());
        // remove the enchantment from the fake container if it is carry weight boost
        for (const auto& effect : enchantment->effects) {
            logger::trace("Effect: {}", effect->baseEffect->GetName());
            logger::trace("PrimaryAV: {}", effect->baseEffect->data.primaryAV);
            logger::trace("SecondaryAV: {}", effect->baseEffect->data.secondaryAV);
            if (effect->baseEffect->data.primaryAV == RE::ActorValue::kCarryWeight) {
                logger::trace("Removing enchantment: {}", effect->baseEffect->GetName());
                // effect->baseEffect = empty_mgeff;
                effect->effectItem.magnitude = std::min<float>(effect->effectItem.magnitude, 0);
            }
        }
    }
}

bool Manager::HandleRegistration(RE::TESObjectREFR* a_container) {
    // Create the fake container form. (<0.7.1): and put in the unownedchestOG
    // extradata gets updates when the player picks up the real container and gets the fake container from unownedchestOG (<0.7.1)

    current_container = a_container;

    // get the source corresponding to the container that we are activating
    if (const auto src = GetContainerSource(a_container->GetBaseObject()->GetFormID())) {
        if (const auto container_refid = a_container->GetFormID(); // register the container to the source data if it is not registered
            !Functions::containsValue(src->data,container_refid)) {
            const auto ChestObjRef = FindNotMatchedChest(); // Not registered. lets find a chest to register it to
            const auto ChestRefID = ChestObjRef->GetFormID();
            logger::info("Matched chest with refid: {} with container with refid: {}", ChestRefID, container_refid);
            if (!src->data.insert({ChestRefID, container_refid}).second) {
			    logger::error("Failed to insert chest refid {} and container refid {} into source data.", ChestRefID,
				    container_refid);
			    return false;
            }
            // add to ChestToFakeContainer
			if (const auto fake_formid = CreateFakeContainer(a_container->GetObjectReference(), ChestRefID, nullptr); !fake_formid) {
				return false;
			}
            else if (!ChestToFakeContainer.insert({ChestRefID, {.outerKey= src->formid, .innerKey= fake_formid}}).second) {
                RaiseMngrErr("Failed to insert chest refid and fake container refid into ChestToFakeContainer.");
                return false;
            }

            // (>=0.7.1) makes a copy of the real at its current state and sends it to the linked chest
            const auto temp_realref = WorldObject::DropObjectIntoTheWorld(a_container->GetBaseObject(), 1);
            if (!temp_realref) {
                RaiseMngrErr("Failed to drop real container into the world");
			    return false;
            }
            if (!xData::UpdateExtras(a_container, temp_realref)) logger::warn("Failed to update extras");
            if (!MoveObject(temp_realref, ChestObjRef, false)) {
                RaiseMngrErr("Failed to remove real container from unownedchestOG");
                return false;
            }
            if (const auto initial_items_map = src->initial_items; !initial_items_map.empty()) {
                SKSE::GetTaskInterface()->AddTask([ChestRefID, initial_items_map] {
                    logger::trace("Adding initial items to chest");
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
        }
        // fake counterparti unownedchestOG de olmayabilir (<0.7.1)
        // cunku load gameden sonra runtimeda halletmem gerekiyo. ekle (<0.7.1)
        // if it is registered, we expect its fake counterpart to exist. Make sure via DFT:
        else {
            const auto chest_refid = GetRealContainerChestID(container_refid);
            const auto real_cont_id = ChestToFakeContainer[chest_refid].outerKey;
            const auto real_cont_editorid = GetEditorID(real_cont_id);
            if (real_cont_editorid.empty()) {
                RaiseMngrErr("Failed to get editorid of real container.");
			    return false;
            }
            // we don't care about updating other stuff at this stage since we will do it in "Take" button
            auto* DFT = DynamicFormTracker::GetSingleton();
            if (auto fake_cont_id = DFT->Fetch(real_cont_id, real_cont_editorid, chest_refid);!fake_cont_id) {
                logger::info("Fake container NOT found in DFT.");
                const auto real_container_obj = RE::TESForm::LookupByID<RE::TESBoundObject>(src->formid);
                if (fake_cont_id = CreateFakeContainer(real_container_obj, chest_refid, nullptr); !fake_cont_id) {
				    RaiseMngrErr("Failed to create fake container.");
				    return false;
				}
                ChestToFakeContainer[chest_refid].innerKey = fake_cont_id;
            } else {
                DFT->EditCustomID(fake_cont_id, chest_refid);
            }
        }
	    return true;
    }

	current_container = nullptr;

	return false;

}

void Manager::MsgBoxCallback(const int result) {
    logger::trace("Result: {}", result);

    if (result != 0 && result != 1 && result != 2 && result != 3) return;

    // More
    if (result == 2) {
        return MsgBoxesNotifs::ShowMessageBox("...", buttons_more, [this](const int res) { this->MsgBoxCallbackMore(res); });
    }

    if (result == 3 || result == 1){
        const auto real_formid = current_container->GetBaseObject()->GetFormID();
        const auto chest = GetRealContainerChest(current_container);
        if (!chest) return RaiseMngrErr("MsgBoxCallback Chest not found");
        SendReal(real_formid, chest);
        // erase real_formid from vector reals_to_sendback
        real_to_sendback = {0,0};

    }

    // Close
    if (result == 3) return;
        
    // Take
    if (result == 1) {
		logger::info("Take");
		current_container->ActivateRef(RE::PlayerCharacter::GetSingleton(),0,nullptr,1,false);

        //RE::PlayerCharacter::GetSingleton()->PickUpObject(current_container,1);
        current_container = nullptr;
        return;
    }

    // Opening container

    // Listen for menu close
    //listen_menuclose = true;

    // Activate the unowned chest
    if (const auto chest = GetRealContainerChest(current_container)) {
		const auto chest_refid = chest->GetFormID();
        const auto fake_id = ChestToFakeContainer[chest_refid].innerKey;
        const auto chest_rename = renames.contains(fake_id) ? renames[fake_id].c_str() : current_container->GetName();
        ActivateChest(chest, chest_rename);
        real_to_sendback = {current_container->GetBaseObject()->GetFormID(), chest_refid};
    }
    else {
		logger::critical("Chest not found.");
    }
}

void Manager::MsgBoxCallbackMore(const int result) {
    logger::trace("More. Result: {}", result);

    if (result != 0 && result != 1 && result != 2 && result != 3) return;

    // Rename
    if (result == 0) {
            
        if (!uiextensions_is_present) return MsgBoxCallback(3);
            
        // Thanks and credits to Bloc: https://discord.com/channels/874895328938172446/945560222670393406/1093262407989731338
        const auto skyrimVM = RE::SkyrimVM::GetSingleton();
        if (const auto vm = skyrimVM ? skyrimVM->impl : nullptr) {
            RE::TESForm* emptyForm = nullptr;
            RE::TESForm* emptyForm2 = nullptr;
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
            const char* menuID = "UITextEntryMenu";
            const char* container_name =
                RE::TESForm::LookupByID<RE::TESBoundObject>(
                    ChestToFakeContainer[GetRealContainerChest(current_container)->GetFormID()].innerKey)
                ->GetName();
            const char* property_name = "text";
            const auto args = RE::MakeFunctionArguments(std::move(menuID), std::move(emptyForm), std::move(emptyForm2));
            const auto args2 =
                RE::MakeFunctionArguments(std::move(menuID), std::move(property_name), std::move(container_name));
            if (vm->DispatchStaticCall("UIExtensions", "SetMenuPropertyString", args2, callback)) {
                if (vm->DispatchStaticCall("UIExtensions", "OpenMenu", args, callback)) listen_menu_close.store(true);
            }
        }
        return;
    }

    // Close
    if (result == 3) return;

    // Back
    if (result == 2) return PromptInterface();

    Uninstall();

}

void Manager::PromptInterface() {

    logger::trace("PromptInterface");

    // get the source corresponding to the container that we are activating
    const auto src = GetContainerSource(current_container->GetBaseObject()->GetFormID());
    if (!src) return RaiseMngrErr("Could not find source for container");
        
    const auto chest = GetRealContainerChest(current_container);
    const auto fake_id = ChestToFakeContainer[chest->GetFormID()].innerKey;

    const std::string name = renames.contains(fake_id) ? renames[fake_id] : current_container->GetDisplayFullName();

    // Round the float to 2 decimal places
    std::ostringstream stream1;
    stream1 << std::fixed << std::setprecision(2) << chest->GetWeightInContainer();
    std::ostringstream stream2;
    stream2 << std::fixed << std::setprecision(2) << src->capacity;

    const auto stream1_str = stream1.str();
    const auto stream2_str = src->capacity > 0 ? "/" + stream2.str() : ""; 

    return MsgBoxesNotifs::ShowMessageBox(
        name + " | W: " + stream1_str + stream2_str + " | V: " + std::to_string(GetChestValue(chest)),
        buttons,
        [this](const int result) { this->MsgBoxCallback(result); });
}

void Manager::LinkExternalContainer(const FormID fakecontainer, const RefID externalcontainer_refid) {

    listen_container_change.store(false);

    logger::trace("LinkExternalContainer");

    const auto external_cont = RE::TESForm::LookupByID<RE::TESObjectREFR>(externalcontainer_refid);
    if (!external_cont) return RaiseMngrErr("External container not found.");

    if (!external_cont->HasContainer()) {
        return RaiseMngrErr("External container does not have a container.");
    }

    logger::trace("Linking external container.");
    const auto chest_refid = GetFakeContainerChest(fakecontainer);
    const auto src = GetContainerSource(ChestToFakeContainer[chest_refid].outerKey);
    if (!src) return RaiseMngrErr("Source not found.");
    src->data[chest_refid] = externalcontainer_refid;

    // if external container is one of ours (bcs of weight limit):
    if (IsChest(externalcontainer_refid) && src->capacity > 0) {
        logger::info("External container is one of our unowneds.");
        if (const auto weight_limit = src->capacity; external_cont->GetWeightInContainer() > weight_limit) {
            RemoveItem(external_cont, player_ref, fakecontainer,
                              RE::ITEM_REMOVE_REASON::kStoreInContainer);
            src->data[chest_refid] = chest_refid;
        }
    }

    // if successfully transferred to the external container, check if the fake container is faved
    if (src->data[chest_refid] != chest_refid &&
        Inventory::IsFavorited(RE::TESForm::LookupByID<RE::TESBoundObject>(fakecontainer), external_cont)) {
        logger::trace("Faved item successfully transferred to external container.");
        external_favs.push_back(fakecontainer);
            
        // also update the extras in the unowned
        const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
        if (!chest) return RaiseMngrErr("LinkExternalContainer: Chest not found");
        if (!UpdateExtrasInInventory(external_cont, fakecontainer, chest,
                                     ChestToFakeContainer[chest_refid].outerKey)) {
            logger::error("Failed to update extras in linkexternal.");
        }
    }

    listen_container_change.store(true);

}

void Manager::InspectItemTransfer(const RefID chest_refid, const FormID item_id) {
    logger::trace("InspectItemTransfer");
    // check if container has enough capacity
    const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
    const auto src = GetContainerSource(ChestToFakeContainer[chest_refid].outerKey);
    if (!src) return RaiseMngrErr("Could not find source for container");
    if (src->capacity<=0) return;
    const auto weight_limit = src->capacity;

    const auto current_weight = chest->GetWeightInContainer();
    auto excess_weight = current_weight - weight_limit;

    if (excess_weight <= 0) return;

    const auto inventory = chest->GetInventory();
    // prioritize the item that was recently added
    int temp_count = 0;
	auto item_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(item_id);
    if (!item_bound || !inventory.contains(item_bound)) {
		logger::critical("Supposedly recently added item not found in inventory.");
        return;
    }
	const auto max_count = inventory.find(item_bound)->second.first;
	const auto item_weight = item_bound->GetWeight();
	if (item_weight <= 0.001) return;
    while (excess_weight > 0 && max_count>temp_count) {
		excess_weight -= item_weight;
		temp_count++;
    }

    const std::pair<RE::TESBoundObject*, Count> recent_item_to_remove = std::make_pair(item_bound, temp_count);

    std::vector<std::pair<RE::TESBoundObject*,Count>> items_to_remove;
    for (const auto& [item_obj, snd] : inventory) {
		if (item_obj->GetFormID() == item_id) continue;
        const auto item_weight_= item_bound->GetWeight();
        if (item_weight_<=0.001) continue;
		const auto max_count_= snd.first;
		if (max_count_ <= 0) continue;
        int temp_count_ = 0;
        while (excess_weight > 0 && max_count_>temp_count_) {
		    excess_weight -= item_weight_;
			temp_count_++;
        }
		if (temp_count_>0) items_to_remove.emplace_back(item_obj, temp_count_);
	}

    std::ranges::sort(items_to_remove,
                      [](const auto& a, const auto& b) {
                          return a.first->GetWeight() > b.first->GetWeight();
                      });

    listen_container_change.store(false);

	if (recent_item_to_remove.second > 0) {
	    chest->RemoveItem(recent_item_to_remove.first, recent_item_to_remove.second, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, player_ref);
        RE::DebugNotification(
                std::format("{} is fully packed! Putting {} {} back.", chest->GetDisplayFullName(),recent_item_to_remove.second,
                            recent_item_to_remove.first->GetName()).c_str());
	}
	for (const auto& [item_obj, count] : items_to_remove) {
		chest->RemoveItem(item_obj, count, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, player_ref);
		RE::DebugNotification(
			std::format("{} is fully packed! Putting {} {} back.", chest->GetDisplayFullName(), count, item_obj->GetName()).c_str());
	}
    listen_container_change.store(true);

}

RE::ObjectRefHandle Manager::RemoveItem(RE::TESObjectREFR* moveFrom, RE::TESObjectREFR* moveTo, const FormID item_id,
                                               const RE::ITEM_REMOVE_REASON reason) {

    logger::trace("RemoveItem");

    auto ref_handle = RE::ObjectRefHandle();

	auto* item_obj = RE::TESForm::LookupByID<RE::TESBoundObject>(item_id);
	if (!item_obj) {
		logger::error("Item object is null");
		return ref_handle;
	}

    if (!moveFrom && !moveTo) {
        logger::critical("moveFrom and moveTo are both null!");
        return ref_handle;
    }
    if (moveFrom && moveTo && moveFrom->GetFormID() == moveTo->GetFormID()) {
        logger::info("moveFrom and moveTo are the same!");
        return ref_handle;
    }

    logger::trace("Removing item reverse");

    listen_container_change.store(false);

	const auto inventory = moveFrom->GetInventory();
	if (!inventory.contains(item_obj)) {
		logger::warn("Item not found in inventory");
		return ref_handle;
	}
	const auto item = inventory.find(item_obj);

    const auto inv_data = item->second.second.get();
    if (const auto asd = inv_data ? inv_data->extraLists : nullptr; !asd || asd->empty()) {
        ref_handle = moveFrom->RemoveItem(item_obj, 1, reason, nullptr, moveTo);
    } else {
        ref_handle = moveFrom->RemoveItem(item_obj, 1, reason, asd->front(), moveTo);
    }
    listen_container_change.store(true);
    return ref_handle;

}

bool Manager::PickUpItem(RE::TESObjectREFR* item, const unsigned int max_try) {
    logger::trace("PickUpItem");

    if (!item) {
        logger::warn("Item is null");
        return false;
    }
    RE::Actor* actor = RE::PlayerCharacter::GetSingleton();
    if (!actor) {
        logger::warn("PlayerCharacter is null");
        return false;
    }

    listen_container_change.store(false);

    const auto item_bound = item->GetObjectReference();
    if (!item_bound) {
        logger::warn("Item bound is null");
        return false;
    }
    const auto item_count = Inventory::GetItemCount(item_bound, actor->GetInventory());
    logger::trace("Item count: {}", item_count);

    for (const auto& x_i : Settings::xRemove) {
        item->extraList.RemoveByType(static_cast<RE::ExtraDataType>(x_i));
    }

    item->extraList.SetOwner(RE::TESForm::LookupByID(0x07));

    unsigned int i = 0;
    while (i < max_try) {
        logger::trace("Critical: PickUpItem");
        actor->PickUpObject(item, 1, false, false);
        logger::trace("Item picked up. Checking if it is in inventory...");
        if (const auto new_item_count = Inventory::GetItemCount(item_bound, actor->GetInventory()); new_item_count > item_count) {
            logger::trace("Item picked up. Took {} extra tries.", i);
            listen_container_change.store(true);
            return true;
        } else logger::trace("item count: {}", new_item_count);
        i++;
    }

    listen_container_change.store(true);

    return false;
}

bool Manager::MoveObject(RE::TESObjectREFR* ref, RE::TESObjectREFR* move2container, const bool owned) {

    logger::trace("MoveObject");

    if (!ref) {
        logger::error("Object is null");
        return false;
    }
    if (!move2container) {
        logger::error("move2container is null");
        return false;
    }
    if (ref->IsDisabled()) logger::warn("Object is disabled");
    if (ref->IsDeleted()) logger::warn("Object is deleted");
        
    // Remove object from world
    if (owned) ref->extraList.SetOwner(RE::TESForm::LookupByID<RE::TESForm>(0x07));
	const auto player = RE::PlayerCharacter::GetSingleton();
	auto ref_bound = ref->GetObjectReference();
    const auto item_count = player->GetItemCount(ref_bound);
	player->PickUpObject(ref, 1);
    if (player->GetItemCount(ref_bound) != item_count+1) {
        logger::error("Item not found in inventory");
        return false;
    }
    player->RemoveItem(ref_bound, 1, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, move2container);
    if (!Inventory::HasItem(ref_bound, move2container)) {
        logger::error("Real container not found in move2container");
        return false;
    }
        
    return true;
}

void Manager::UpdateFakeWV(RE::TESBoundObject* fake_form, RE::TESObjectREFR* chest_linked, const float weight_ratio) {
    logger::trace("pre-UpdateFakeWV");
    if (!fake_form) return RaiseMngrErr("Fake form is null");
    std::string formtype(RE::FormTypeToString(fake_form->GetFormType()));
    if (formtype == "SCRL") UpdateFakeWV<RE::ScrollItem>(fake_form->As<RE::ScrollItem>(), chest_linked, weight_ratio);
    else if (formtype == "ARMO") UpdateFakeWV<RE::TESObjectARMO>(fake_form->As<RE::TESObjectARMO>(), chest_linked, weight_ratio);
    else if (formtype == "BOOK") UpdateFakeWV<RE::TESObjectBOOK>(fake_form->As<RE::TESObjectBOOK>(), chest_linked, weight_ratio);
    else if (formtype == "INGR") UpdateFakeWV<RE::IngredientItem>(fake_form->As<RE::IngredientItem>(), chest_linked, weight_ratio);
    else if (formtype == "MISC") UpdateFakeWV<RE::TESObjectMISC>(fake_form->As<RE::TESObjectMISC>(), chest_linked, weight_ratio);
    else if (formtype == "WEAP") UpdateFakeWV<RE::TESObjectWEAP>(fake_form->As<RE::TESObjectWEAP>(), chest_linked, weight_ratio);
    else if (formtype == "AMMO") UpdateFakeWV<RE::TESAmmo>(fake_form->As<RE::TESAmmo>(), chest_linked, weight_ratio);
    else if (formtype == "SLGM") UpdateFakeWV<RE::TESSoulGem>(fake_form->As<RE::TESSoulGem>(), chest_linked, weight_ratio);
    else if (formtype == "ALCH") UpdateFakeWV<RE::AlchemyItem>(fake_form->As<RE::AlchemyItem>(), chest_linked, weight_ratio);
    else RaiseMngrErr(std::format("Form type not supported: {}", formtype));
        
}

bool Manager::UpdateExtrasInInventory(RE::TESObjectREFR* from_inv, const FormID from_item_formid,
                                      RE::TESObjectREFR* to_inv, const FormID to_item_formid) {
    logger::trace("UpdateExtrasInInventory");
    const auto from_item = RE::TESForm::LookupByID<RE::TESBoundObject>(from_item_formid);
    const auto to_item = RE::TESForm::LookupByID<RE::TESBoundObject>(to_item_formid);
    if (!from_item || !to_item) {
        logger::error("Item bound is null");
        return false;
    }
    if (!Inventory::HasItem(from_item, from_inv) || !Inventory::HasItem(to_item, to_inv)) {
        logger::error("Item not found in inventory");
        return false;
    }
    const auto inventory_from = from_inv->GetInventory();
    const auto inventory_to = to_inv->GetInventory();
    const auto entry_from = inventory_from.find(from_item);
    const auto entry_to = inventory_to.find(to_item);
    RE::ExtraDataList* extralist_from;
    RE::ExtraDataList* extralist_to;
    RE::TESObjectREFR* ref_to = nullptr;
    bool removed_from = false;
    bool removed_to = false;
    if (entry_from->second.second && entry_from->second.second->extraLists &&
        !entry_from->second.second->extraLists->empty() && entry_from->second.second->extraLists->front()) {
        extralist_from = entry_from->second.second->extraLists->front();
    } else {
        logger::warn("No extra data list found in from item in inventory");
        return true;
        /*auto from_refhandle =
                RemoveItem(from_inv, nullptr, from_item_formid, RE::ITEM_REMOVE_REASON::kDropping);
            if (!from_refhandle) {
                logger::error("Failed to remove item from inventory (from)");
                return false;
            }
            logger::trace("from_refhandle.get().get()");
            removed_from = true;
            ref_from = from_refhandle.get().get();
            extralist_from = &from_refhandle.get()->extraList;
            logger::trace("extralist_from");*/
    }
    if (!extralist_from) {
        logger::warn("Extra data list is null (from)");
        return true;
    }

    if (entry_to->second.second && entry_to->second.second->extraLists &&
        !entry_to->second.second->extraLists->empty() && entry_to->second.second->extraLists->front()) {
        extralist_to = entry_to->second.second->extraLists->front();
    } else {
        logger::warn("No extra data list found in to item in inventory");
        const auto to_refhandle = RemoveItem(to_inv, nullptr, to_item_formid, RE::ITEM_REMOVE_REASON::kDropping);
        if (!to_refhandle) {
            logger::error("Failed to remove item from inventory (to)");
            return false;
        }
        logger::trace("to_refhandle.get().get()");
        removed_to = true;
        ref_to = to_refhandle.get().get();
        extralist_to = &ref_to->extraList;
        logger::trace("extralist_to");
    }

    if (!extralist_to) {
        logger::error("Extra data list is null (to)");
        return false;
    }

    logger::trace("Updating extras");
    if (!xData::UpdateExtras(extralist_from, extralist_to)) {
        logger::error("Failed to update extras");
        return false;
    }
    logger::trace("Updated extras");

    if (RE::TESObjectREFR* ref_from = nullptr; removed_from && !MoveObject(ref_from, from_inv)) return false;
    if (removed_to && !MoveObject(ref_to, to_inv)) return false;

    return true;
}

void Manager::HandleFormDelete_(const RefID chest_refid) {

    std::unique_lock lock(sharedMutex_);

    auto real_formid = ChestToFakeContainer[chest_refid].outerKey;
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
    const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
    const auto fake_formid = ChestToFakeContainer[chest_refid].innerKey;
    if (chest) DeRegisterChest(chest_refid);
    else MsgBoxesNotifs::InGame::CustomMsg("Could not return your items.");
    RemoveItem(player_ref, nullptr, fake_formid, RE::ITEM_REMOVE_REASON::kRemove);
}

void Manager::Reset() {
    logger::info("Resetting manager...");
    for (auto& src : sources) {
        src.data.clear();
    }
    ChestToFakeContainer.clear(); // we will update this in ReceiveData
    external_favs.clear(); // we will update this in ReceiveData
    renames.clear(); // we will update this in ReceiveData
    handled_external_conts.clear();
    Clear();
    //handled_external_conts.clear();
    current_container = nullptr;
    listen_menu_close.store(false);
    listen_activate.store(true);
    listen_container_change.store(true);
    isUninstalled.store(false);
    logger::info("Manager reset.");
}

void Manager::Print() {
        
    for (const auto& src : sources) { 
        if (!src.data.empty()) {
            logger::trace("Printing............Source formid: {:x}", src.formid);
            Functions::printMap(src.data);
        }
    }
    for (const auto& [chest_ref, cont_ref] : ChestToFakeContainer) {
        logger::trace("Chest refid: {:x}, Real container formid: {:x}, Fake container formid: {:x}", chest_ref, cont_ref.outerKey, cont_ref.innerKey);
    }
}

void Manager::SendData() {

    logger::info("--------Sending data---------");
    Print(); 
    Clear();
    int no_of_container = 0;
    for (auto& src : sources) {
        for (const auto& [chest_ref, cont_ref] : src.data) {
            no_of_container++;
            bool is_equipped_x = false;
            bool is_favorited_x = false;
            if (!chest_ref) return RaiseMngrErr("Chest refid is null");
            auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;
            if (chest_ref == cont_ref) {
                const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
                is_equipped_x = Inventory::IsEquipped(fake_bound);
                is_favorited_x = Inventory::IsFavorited(fake_bound,player_ref);
                if (const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref); !chest) return RaiseMngrErr("Chest not found");
                /*if (!UpdateExtrasInInventory(player_ref, fake_formid, chest, src.formid)) {
						logger::error("Failed to update extras in player's inventory.");
					}*/
            } 
            // check if the fake container is faved in an external container
            else {
                if (auto it = std::ranges::find(external_favs, fake_formid); it != external_favs.end()) is_favorited_x = true;
            }
            const auto rename_ = renames.contains(fake_formid) ? renames[fake_formid] : "";
            FormIDX fake_container_x(ChestToFakeContainer[chest_ref].innerKey, is_equipped_x, is_favorited_x, rename_);
            SetData({src.formid, chest_ref}, {fake_container_x, cont_ref});
        }
    }
    logger::info("Data sent. Number of containers: {}", no_of_container);
}

void Manager::ReceiveData() {
    logger::info("--------Receiving data---------");

    //std::lock_guard<std::mutex> lock(mutex);



    /*if (DFT->GetNDeleted() > 0) {
            logger::critical("ReceiveData: Deleted forms exist.");
            return RaiseMngrErr("ReceiveData: Deleted forms exist.");
        }*/

    listen_container_change.store(false);

    std::map<RefID,std::pair<bool,bool>> chest_equipped_fav;

    std::map<RefID, FormFormID> unmathced_chests;
    for (const auto& [realcontForm_chestRef, fakecontForm_contRef] : m_Data) {

        bool no_match = true;
        FormID realcontForm = realcontForm_chestRef.outerKey;
        RefID chestRef = realcontForm_chestRef.innerKey;
        FormIDX fakecontForm = fakecontForm_contRef.outerKey;
        RefID contRef = fakecontForm_contRef.innerKey;

        for (auto& src : sources) {
            if (realcontForm != src.formid) continue;
            if (!src.data.insert({chestRef, contRef}).second) {
                return RaiseMngrErr(
                    std::format("RefID {:x} or RefID {:x} at formid {:x} already exists in sources data.", chestRef,
                                contRef, realcontForm));
            }
            if (!ChestToFakeContainer.insert({chestRef, {.outerKey= realcontForm, .innerKey= fakecontForm.id}}).second) {
                return RaiseMngrErr(
                    std::format("realcontForm {:x} with fakecontForm {:x} at chestref {:x} already exists in "
                                "ChestToFakeContainer.",
                                chestRef, realcontForm, fakecontForm.id));
            }
            if (!fakecontForm.name.empty()) renames[fakecontForm.id] = fakecontForm.name;
            if (chestRef == contRef) chest_equipped_fav[chestRef] = {fakecontForm.equipped, fakecontForm.favorited};
            else if (fakecontForm.favorited) external_favs.push_back(fakecontForm.id);
            no_match = false;
            break;
        }
        if (no_match) unmathced_chests[chestRef] = {.outerKey= realcontForm, .innerKey= fakecontForm.id};
    }

    // handle the unmathced chests
    // user probably changed the INI. we try to retrieve the items.
    for (const auto& [chestRef_, RealFakeForm_] : unmathced_chests) {
        logger::warn("FormID {:x} not found in sources.", RealFakeForm_.outerKey);
        if (other_settings[Settings::otherstuffKeys[0]]) {
            MsgBoxesNotifs::InGame::ProblemWithContainer(RealFakeForm_.outerKey);
        }
        logger::info("Deregistering chest");

        const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chestRef_);
        if (!chest) return RaiseMngrErr("Chest not found");
        RemoveAllItemsFromChest(chest, player_ref);
        // also remove the associated fake item from player or unowned chest
        if (RealFakeForm_.innerKey) {
			//RemoveItem(player_ref, nullptr, fake_id, RE::ITEM_REMOVE_REASON::kRemove); // causes crash
            //RemoveItem(unownedChestOG, nullptr, fake_id, RE::ITEM_REMOVE_REASON::kRemove); // < v0.7.1
        }
        // make sure no item is left in the chest
        if (!chest->GetInventory().empty()) {
            logger::critical("Chest still has items in it. Degistering failed");
            MsgBoxesNotifs::InGame::CustomMsg("Items might not have been retrieved successfully.");
        }

        m_Data.erase({RealFakeForm_.outerKey, chestRef_});
    }

#ifndef NDEBUG
    Print();
#endif

    auto* DFT = DynamicFormTracker::GetSingleton();

    // Now i need to iterate through the chests deal with some cases
    std::vector<RefID> handled_already;
    for (const auto& chest_ref : ChestToFakeContainer | std::views::keys) {
        if (std::ranges::find(handled_already, chest_ref) != handled_already.end()) {
            continue;
        }
        Something2(chest_ref,handled_already);
        const auto _real_fid = ChestToFakeContainer[chest_ref].outerKey;
        const auto real_editorid = GetEditorID(_real_fid);
        if (real_editorid.empty()) {
            logger::critical("Real container with formid {:x} has no editorid.", _real_fid);
            return RaiseMngrErr("Real container has no editorid.");
        }
        const auto _fake_fid = ChestToFakeContainer[chest_ref].innerKey;
        DFT->Reserve(_real_fid, real_editorid, _fake_fid);
    }

    // print handled_already
    logger::info("handled_already: ");
    for (const auto& ref : handled_already) {
        logger::info("{}", ref);
    }


    handled_already.clear();

    // I make the fake containers in player inventory equipped/favorited:
    logger::trace("Equipping and favoriting fake containers in player's inventory");
    const auto inventory_changes = player_ref->GetInventoryChanges();
    const auto entries = inventory_changes->entryList;
    for (auto it = entries->begin(); it != entries->end(); ++it){
        if (!(*it)) {
            logger::error("Entry is null. Fave-equip failed.");
            continue;
        } 
        if (!(*it)->object) {
            logger::error("Object is null. Fave-equip failed.");
            continue;
        }
        if (auto fake_formid = (*it)->object->GetFormID(); IsFakeContainer(fake_formid)) {
			const auto fakecontainerchestid = GetFakeContainerChest(fake_formid);
            const auto [is_equipped_x,is_faved_x ] = chest_equipped_fav[fakecontainerchestid];
            if (is_equipped_x) {
                logger::trace("Equipping fake container with formid {:x}", fake_formid);
                Inventory::EquipItem((*it)->object);
                /*RE::ActorEquipManager::GetSingleton()->EquipObject(player_ref->As<RE::Actor>(), (*it)->object, 
                        nullptr,1,(const RE::BGSEquipSlot*)nullptr,true,false,false,false);*/
            }
            if (is_faved_x) {
                logger::trace("Favoriting fake container with formid {:x}", fake_formid);
                Inventory::FavoriteItem((*it)->object,player_ref);
                //inventory_changes->SetFavorite((*it), (*it)->extraLists->front());
            }
			if (const auto src = GetContainerSource(ChestToFakeContainer[fakecontainerchestid].outerKey)) {
				if (const auto fakecontainer_chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(fakecontainerchestid)) {
                    UpdateFakeWV((*it)->object,fakecontainer_chest,src->weight_ratio);
				}
			}
        }
    }

    for (const auto& source : sources) {
        for (auto dyn_formid : DFT->GetFormSet(source.formid, source.editorid)) {
			const auto editorid = source.editorid.empty() ? GetEditorID(source.formid) : source.editorid;
            DFT->Reserve(source.formid,editorid ,dyn_formid);
			logger::trace("Reserving formid {:x} for source formid {:x} and editorid {}", dyn_formid, source.formid, source.editorid);
        }
    }
    // need to get rid of the dynamic forms which are unused
    logger::trace("Deleting unused fake forms from bank.");
    listen_container_change.store(false);
    DFT->DeleteInactives();
    listen_container_change.store(true);
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
    Print();
        
    current_container = nullptr;
    listen_container_change.store(true);
}