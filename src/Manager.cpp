#include "Manager.h"
#include <ranges>

#include "Papyrus.h"

void Manager::SendReal(RE::TESBoundObject* real_obj, RE::TESObjectREFR* chest) {
    const auto unownedChestOG = RE::TESForm::LookupByID<RE::TESObjectREFR>(unownedChestOGRefID);
    if (!unownedChestOG) return RaiseMngrErr("MsgBoxCallback unownedChestOG is null");
    if (real_obj && !Inventory::HasItem(real_obj, unownedChestOG)){
        return RaiseMngrErr("Real container not found in unownedChestOG");
    }
    unownedChestOG->RemoveItem(real_obj,1,RE::ITEM_REMOVE_REASON::kStoreInContainer,nullptr,chest);
}

RE::TESBoundObject* Manager::FakeToRealContainer(const FormID fake) {

	std::shared_lock lock(chest2fake_mutex_);
    for (const auto& cont_forms : ChestToFakeContainer | std::views::values) {
        if (cont_forms.innerKey == fake) {
            return RE::TESForm::LookupByID<RE::TESBoundObject>(cont_forms.outerKey);
        }
    }
    return nullptr;
}

RefID Manager::GetRealContainerChestID(const RefID real_refid) const {
	std::shared_lock lock(source_mutex_);
    for (const auto& src : sources) {
        for (const auto& [chest_refid, cont_refid] : src.data) {
            if (cont_refid == real_refid) return chest_refid;
        }
    }
    return 0;
}

RE::TESBoundObject* Manager::GetFakeBound(const RefID chest_id) const {
	std::shared_lock lock(chest2fake_mutex_);
	if (ChestToFakeContainer.contains(chest_id)) {
		return RE::TESForm::LookupByID<RE::TESBoundObject>(ChestToFakeContainer.at(chest_id).innerKey);
	}
    return nullptr;
}

FormID Manager::GetFakeID(const RefID chest_id) const
{
	std::shared_lock lock(chest2fake_mutex_);
	if (ChestToFakeContainer.contains(chest_id)) {
		return ChestToFakeContainer.at(chest_id).innerKey;
	}
	return 0;
}

FormID Manager::GetRealID(const RefID chest_id) const
{
	std::shared_lock lock(chest2fake_mutex_);
	if (ChestToFakeContainer.contains(chest_id)) {
		return ChestToFakeContainer.at(chest_id).outerKey;
	}
	return 0;
}

void Manager::OnPickup(RE::TESObjectREFR* picked_up_by, RE::TESObjectREFR* a_object)
{
    if (const auto chest = GetRealContainerChest(a_object)) {
	    const auto src = GetContainerSource(a_object->GetBaseObject()->GetFormID());
		const auto chest_refid = chest->GetFormID();
        if (auto* fake_bound = GetFakeBound(chest_refid)) {
			const auto fake_id = fake_bound->GetFormID();
			src->data.at(chest_refid) = picked_up_by->GetFormID();
            WorldObject::SwapObjects(a_object, fake_bound,false);
            UpdateFakeWV(fake_bound, chest, src->weight_ratio);
            if (other_settings[Settings::otherstuffKeys[1]]) {
				auto ref_handle = picked_up_by->GetHandle();
			    SKSE::GetTaskInterface()->AddTask([this, fake_id, ref_handle]() {
                    if (const auto ref = ref_handle.get()) {
                        RemoveCarryWeightBoost(fake_id, ref.get());
                    }
			    });
            }
        }
		else {
			logger::critical("Fake bound not found.");
		}
	}
}

void Manager::HandleDrop(RE::TESObjectREFR* fake_object)
{
	bool found = false;
	std::shared_lock lock(chest2fake_mutex_);
	for (const auto& [chest_refid, real_fake_id] : ChestToFakeContainer) {
		if (real_fake_id.innerKey == fake_object->GetBaseObject()->GetFormID()) {
			const auto real_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(real_fake_id.outerKey);
			WorldObject::SwapObjects(fake_object, real_bound, false);
            UpdateData(chest_refid,fake_object->GetFormID());
			found = true;
            break;
		}
	}

    if (!found) {
		logger::warn("Fake object not found in ChestToFakeContainer.");
		if (const auto baseform = DynamicFormTracker::GetSingleton()->GetOGFormOfDynamic(fake_object->GetBaseObject()->GetFormID())) {
	        WorldObject::SwapObjects(fake_object, skyrim_cast<RE::TESBoundObject*>(baseform), false);    
		}
    }
}

void Manager::UpdateData(const RefID chestID, const RefID loc_id)
{
	std::shared_lock lock(chest2fake_mutex_);
    if (ChestToFakeContainer.contains(chestID)) {
		const auto real_id = ChestToFakeContainer.at(chestID).outerKey;
		const auto src = GetContainerSource(real_id);
		std::unique_lock lock2(source_mutex_);
		src->data.at(chestID) = loc_id;
        
    }
    else {
		logger::error("Chest ID not found in ChestToFakeContainer.");
    }
}

void Manager::OnLongPressEquip(const RE::TESBoundObject* a_selected_item)
{
	const auto fake_id = a_selected_item->GetFormID();
	const auto chest_refid = GetFakeContainerChestID(fake_id);
	if (const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid)) {
		if (const auto real_bound = FakeToRealContainer(fake_id)) {
            if (const auto ui = RE::UI::GetSingleton(); ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
				if (!LookupReferenceByHandle(RE::ContainerMenu::GetTargetRefHandle(), containermenu_owner)) {
					containermenu_owner.reset();
				}
            }
            closed_menu = Menu::CloseMenu();
			SKSE::GetTaskInterface()->AddTask([this, fake_id, real_bound, chest, chest_refid]() {
			    if (ActivateChest(chest, renames.contains(fake_id) ? renames.at(fake_id).c_str() : real_bound->GetName())) {
                    queued_real_to_sendback = {real_bound, chest_refid};
			    }
			});
		}
	}
}

bool Manager::ActivateChest(RE::TESObjectREFR* chest, const char* chest_name) const {

    unownedChest->fullName = chest_name;
    /*chest->OpenContainer(0);
	return true;*/
    if (const auto a_obj = chest->GetBaseObject()->As<RE::TESObjectCONT>()) {
        return a_obj->Activate(chest, player_ref, 0, a_obj, 1);
    }
    logger::error("ActivateChest: Chest is not a container.");
    return false;
}

void Manager::HandleCraftingExit() {
    logger::trace("HandleCraftingExit");

    logger::trace("Crafting menu closed");
    for (auto& src : sources) {
        for (const auto& [chest_refid, cont_refid] : src.data) {
            // we trust that the player will leave the crafting menu at some point and everything will be reverted
            if (cont_refid != 0x14) continue;  // playerda deilse continue
            const auto fake_formid = ChestToFakeContainer.at(chest_refid).innerKey;
            const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
            if (!fake_bound) return RaiseMngrErr("Fake bound not found.");
            const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
            if (!chest) return RaiseMngrErr("Chest is null");
            if (!Inventory::HasItem(fake_bound,player_ref)) {
                // it can happen when using arcane enchanter to destroy the item
                logger::info("Player does not have fake item. Probably destroyed in arcane enchanter.");
				const auto real_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(src.formid);
                RemoveItem(chest, nullptr, real_bound, RE::ITEM_REMOVE_REASON::kRemove);
                DeRegisterChest(chest_refid);
                continue;
            }
            if (!UpdateExtrasInInventory(player_ref, fake_formid, chest, src.formid)) {
                logger::error("Failed to update extras in player's inventory.");
                return;
            }
        }
    }
}

void Manager::OnConsume(const FormID fake_formid, RE::TESObjectREFR* consumed_by) {
    // check if player has the fake item
    // sometimes player does not have the fake item but it can still be there with count = 0.
    if (const auto fake_obj = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid)) {

        {
		    // check if its location is actually in "consumed_by" inventory
			const auto chest_id = GetFakeContainerChestID(fake_formid);
            const auto real_id = GetRealID(chest_id);
			std::shared_lock lock(source_mutex_);
			if (const auto src = GetContainerSource(real_id)) {
				if (src->data.at(chest_id) != consumed_by->GetFormID()) {
                    if (!doppelgangers.contains(consumed_by->GetBaseObject()->GetFormID())) {
						logger::error("Fake object is not supposed to be found in consumed_by {:x} {:x}.",consumed_by->GetFormID(),consumed_by->GetBaseObject()->GetFormID());
					}
                    return;
				}
			}
        }

        // the cleanup might actually not be necessary since DeRegisterChest will remove it from consumed_by
        const auto inv = consumed_by->GetInventory();
        if (const auto item_count = inv.contains(fake_obj) ? inv.at(fake_obj).first : 0; item_count>0) {
            if (const auto real_obj = FakeToRealContainer(fake_formid)) {
                const auto chest_refid = GetFakeContainerChestID(fake_formid);
                const auto chest_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);

                // make also sure that the real counterpart is still in unowned
                if (!Inventory::HasItem(real_obj, chest_ref)) return RaiseMngrErr("Real counterpart not found in unowned chest.");
                    
                logger::info("Deregistering bcs Item consumed.");
                RemoveItem(chest_ref, nullptr, real_obj, RE::ITEM_REMOVE_REASON::kRemove);
                DeRegisterChest(chest_refid);
                RE::SendUIMessage::SendInventoryUpdateMessage(player_ref, nullptr);
                
            }
            else {
				logger::error("Real counterpart not found.");
            }
        }
    }
    else {
		logger::error("Fake object not found.");
    }
}

int Manager::GetChestValue(RE::TESObjectREFR* a_chest) {
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

RE::TESObjectREFR* Manager::GetRealContainerChest(const RE::TESObjectREFR* real_container) const {
    if (const auto chest_refid = GetRealContainerChestID(real_container->GetFormID())) {
		return RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
    }
    return nullptr;
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

std::vector<RefID> Manager::GetConnectedChests(const RefID chestID) {
    std::vector<RefID> connected_chests; // chestID nin icindeki chestler
	std::shared_lock lock(chest2fake_mutex_);
    for (const auto& [a_chest_id, cont_ref] : ChestToFakeContainer) {
        if (const auto src = GetContainerSource(cont_ref.outerKey)) {
			std::shared_lock lock2(source_mutex_);
            if (chestID != a_chest_id && src->data.at(a_chest_id) == chestID) {
                connected_chests.push_back(a_chest_id);
            }
        }
		else {
			logger::error("Source not found for formid: {:x}", cont_ref.outerKey);
		}
    }
    return connected_chests;
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
		std::shared_lock lock(source_mutex_);
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

RefID Manager::GetFakeContainerChestID(const FormID fake_id) {

	std::shared_lock lock(chest2fake_mutex_);
    for (const auto& [chest_ref, cont_forms] : ChestToFakeContainer) {
        if (cont_forms.innerKey == fake_id) return chest_ref;
    }
    return 0;
}

std::vector<FormID> Manager::RemoveAllItemsFromChest(RE::TESObjectREFR* chest, RE::TESObjectREFR* move2ref) {

    logger::trace("RemoveAllItemsFromChest");

    std::vector<FormID> removed_objects;

    if (!chest) {
        RaiseMngrErr("Chest is null");
        return removed_objects;
    }

    logger::trace("Checking for fake containers in chest");
    // need to handle if a fake container was inside this chest. yani cont_ref i bu cheste bakan data varsa
    // redirectlemeliyim
    const auto chest_refid = chest->GetFormID();
    std::vector<FormID> connected_chests;
    for (std::shared_lock lock(source_mutex_); const auto& src : sources) {
        for (const auto& [key, value] : src.data) {
            if (value == chest_refid && key != value) {
                logger::info(
                    "Fake container with formid {:x} found in chest during RemoveAllItemsFromChest. Redirecting...",
                    ChestToFakeContainer.at(key).innerKey);
                // the chest that is connected to the fake container which was inside this chest
				connected_chests.push_back(GetFakeID(key));
            }
        }
    }

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

    const auto removeReason = move2ref ? RE::ITEM_REMOVE_REASON::kStoreInContainer : RE::ITEM_REMOVE_REASON::kRemove;
    //if (move2ref && move2ref->IsPlayerRef()) removeReason = RE::ITEM_REMOVE_REASON::kRemove;

    for (const auto inventory = chest->GetInventory(); const auto& item : inventory) {
        const auto item_obj = item.first;
        if (!std::strlen(item_obj->GetName())) logger::warn("RemoveAllItemsFromChest: Item name is empty");
        auto item_count = item.second.first;
        const auto inv_data = item.second.second.get();
        if (const auto asd = inv_data->extraLists; !asd || asd->empty()) {
            //logger::trace("Removing item: {} with count: {} and remove reason", item_obj->GetName(), item_count);
            chest->RemoveItem(item_obj, item_count, removeReason, nullptr, move2ref);
        } else {
            //logger::trace("Removing item with extradata: {} with count: {}", item_obj->GetName(), item_count);
            chest->RemoveItem(item_obj, item_count, removeReason, asd->front(), move2ref);
        }
        removed_objects.push_back(item_obj->GetFormID());
    }
        
        
	for (const auto& connected_chest : connected_chests) {
        HandleSell(connected_chest, move2ref);
	}

    return removed_objects;
}

void Manager::DeRegisterChest(const RefID chest_ref) {

    logger::info("Deregistering chest");

    const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref);
    if (!chest) {
        RaiseMngrErr("Chest not found");
        return;
    }

    const auto src = GetContainerSource(GetRealID(chest_ref));
    if (!src) {
        RaiseMngrErr("Source not found");
        return;
    }

    RemoveAllItemsFromChest(chest, player_ref);

    if (std::unique_lock lock(source_mutex_); !src->data.erase(chest_ref)) {
        RaiseMngrErr("Failed to remove chest refid from source");
        return;
    }
    if (std::unique_lock lock(chest2fake_mutex_); !ChestToFakeContainer.erase(chest_ref)) {
        RaiseMngrErr("Failed to erase chest refid from ChestToFakeContainer");
        return;
    }
    // make sure no item is left in the chest
    if (!chest->GetInventory().empty()) {
        RaiseMngrErr("Chest still has items in it. Degistering failed");
        return;
    }   
}

Source* Manager::GetContainerSource(const FormID real_id) {
	std::shared_lock lock(source_mutex_);
    for (auto& src : sources) {
        if (src.formid == real_id) {
            return &src;
        }
    }
    logger::error("Container source not found");
    return nullptr;
}

bool Manager::HasItemPlusCleanUp(RE::TESBoundObject* item, RE::TESObjectREFR* item_owner) {
    const auto inventory = item_owner->GetInventory();
    if (const auto entry = inventory.find(item); entry == inventory.end()) return false;
    else if (entry->second.first > 0) return true;
	logger::warn("Item count is 0. Removing item.");
    RemoveItem(item_owner, nullptr, item, RE::ITEM_REMOVE_REASON::kRemove);
    return false;
}

void Manager::Uninstall() {

	if (isUninstalled.load()) return;

    bool uninstall_successful = true;

    logger::info("Uninstalling...");
    logger::info("No of chests in cell: {}", GetNoChests());

    std::vector<std::pair<RefID,FormID>> all_chests_fakes;
    // first lets get rid of the fake items from everywhere
    for (std::shared_lock lock(chest2fake_mutex_);
		const auto& [chest_refid, real_fake_formid] : ChestToFakeContainer) {
		all_chests_fakes.emplace_back(chest_refid,real_fake_formid.innerKey);
    }
	logger::info("Removing fake items from player's inventory");

    Reset();

	for (const auto& chest_refid : all_chests_fakes | std::views::keys) {
		if (const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid); !chest) {
			uninstall_successful = false;
			logger::error("Chest not found");
			break;
		}
        else {
            RemoveAllItemsFromChest(chest, player_ref);
        }
	}
    for (const auto& fake_id : all_chests_fakes | std::views::values) {
		if (const auto fake = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_id); !fake) {
			uninstall_successful = false;
			logger::error("Chest not found");
			break;
		}
        else {
			player_ref->RemoveItem(fake, 1, RE::ITEM_REMOVE_REASON::kRemove,nullptr,nullptr);
        }
	}


    // Delete all unowned chests and try to return all items to the player's inventory while doing that
	logger::info("Removing all unowned chests");
	{
	    RE::BSSpinLockGuard locker(unownedCell->GetRuntimeData().spinLock);
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
	}


    logger::info("uninstall_successful: {}", uninstall_successful);
    logger::info("No of chests in cell: {}", GetNoChests());

    if (GetNoChests() != 1) uninstall_successful = false;

    logger::info("uninstall_successful: {}", uninstall_successful);

    if (uninstall_successful) {
        logger::info("Uninstall successful.");
        MsgBoxesNotifs::InGame::UninstallSuccessful();
    } else {
        logger::critical("Uninstall failed.");
        MsgBoxesNotifs::InGame::UninstallFailed();
    }

	DynamicFormTracker::GetSingleton()->DeleteAll();

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

    sources = LoadSources();

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

    const auto unownedChestOG = RE::TESForm::LookupByID<RE::TESObjectREFR>(0x000EA29A);
    unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(unownedChestFormID);
    unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000EA28B);
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

    else {
        uiextensions_is_present = true;
    }

    // doppelganger ccbgssse018-shadowrend.esl

    for (const auto local_id : doppelgangers_local) {
		if (const auto a_form = data_handler->LookupForm(local_id, "ccbgssse018-shadowrend.esl")) {
			doppelgangers.insert(a_form->GetFormID());
		}
	}

    logger::info("Manager initialized.");
}

void Manager::HandleSell(const FormID fake_container, RE::TESObjectREFR* sell_ref) {
    // assumes the sell_refid is a container
    // add the real container to the vendor from the unownedchest
    const auto chest_refid = GetFakeContainerChestID(fake_container);
    if (const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid)) {
		const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_container);
	    logger::trace("HandleSell {}", fake_bound->GetName());
        if (other_settings[Settings::otherstuffKeys[3]]) RemoveAllItemsFromChest(chest, sell_ref);
        else {
			std::shared_lock lock(chest2fake_mutex_);
			const auto real_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(ChestToFakeContainer[chest_refid].outerKey);
            RemoveItem(chest, sell_ref, real_bound,
                RE::ITEM_REMOVE_REASON::kStoreInContainer);
        }
        // remove all items from the chest to the player's inventory and deregister this chest
        DeRegisterChest(chest_refid);
        RemoveItem(sell_ref, nullptr, fake_bound, RE::ITEM_REMOVE_REASON::kRemove);
    }
    else {
		RaiseMngrErr("Chest not found");
    }
}

void Manager::HandleFormDelete(const RefID refid) {
    //std::lock_guard<std::mutex> lock(mutex);
	std::shared_lock lock(chest2fake_mutex_);
    if (ChestToFakeContainer.contains(refid)) return HandleFormDelete_(refid);
    // the deleted reference could also be a real container out in the world.
    // in that case i need to return the items from its chest
	std::shared_lock lock2(source_mutex_);
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
	std::shared_lock lock(source_mutex_);
	return std::ranges::any_of(sources, [formid](const Source& src) { return src.formid == formid; });
}

void Manager::OnActivateContainer(RE::TESObjectREFR* a_container) {

    if (!HandleRegistration(a_container)) return;
        
    // store it temporarily in unownedChestOG
    if (const auto unownedChestOG = RE::TESForm::LookupByID<RE::TESObjectREFR>(unownedChestOGRefID);
        !unownedChestOG) {
        return RaiseMngrErr("OnActivateContainer: unownedChestOG is null");
    }
    else if (const auto chest = GetRealContainerChest(current_container); !chest) {
        return RaiseMngrErr("OnActivateContainer: Chest not found");
    }
    else if (const auto real_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(ChestToFakeContainer.at(chest->GetFormID()).outerKey)) {
        RemoveItem(chest, unownedChestOG, real_bound, RE::ITEM_REMOVE_REASON::kStoreInContainer);
        return PromptInterface();
    }
	return RaiseMngrErr("OnActivateContainer: Real bound not found");
}

void Manager::HandleFakePlacement(RE::TESObjectREFR* external_cont) {
    // if the external container is already handled (handled_external_conts) return
    if (const auto it = std::ranges::find(handled_external_conts, external_cont->GetFormID()); it != handled_external_conts.end()) return;
    if (!external_cont->HasContainer()) return;
    if (IsUnownedChest(external_cont->GetFormID())) return;

	const auto refid = external_cont->GetFormID();
    if (!IsARegistry(refid)) return;

    for (auto& src : sources) {
        for (const auto& [chest_ref, cont_ref] : src.data) {
            if (refid != cont_ref) continue;
            FakePlacement(src.data.at(chest_ref), chest_ref, external_cont);
            // break yok cunku baska fakeler de external_cont un icinde olabilir
        }
    }
    handled_external_conts.push_back(refid);
}

bool Manager::IsFakeContainer(const FormID formid) {
	std::shared_lock lock(chest2fake_mutex_);
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
		logger::error("Current container is null");
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
    else MsgBoxCallback(3);
}

void Manager::OnContainerMenuExit() { 
    if (real_to_sendback.first && real_to_sendback.second) {
        const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(real_to_sendback.second);
        if (!chest) return RaiseMngrErr("OnContainerMenuExit: Chest is null");
        SendReal(real_to_sendback.first, chest);
        const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(ChestToFakeContainer.at(real_to_sendback.second).innerKey);
		fake_bound->formFlags = real_to_sendback.first->formFlags;
		if (other_settings.at(Settings::otherstuffKeys[2])) {
            if (closed_menu == RE::ContainerMenu::MENU_NAME) {
				SKSE::GetTaskInterface()->AddTask([this]() {
                    if (containermenu_owner) {
						logger::trace("Opening container menu");
                        containermenu_owner->OpenContainer(0);
                        containermenu_owner.reset();
                    }
				});
            }
            else {
			    Menu::OpenMenu(closed_menu);
            }
            closed_menu = "";
		}
		UpdateFakeWV(fake_bound);
        real_to_sendback = {nullptr,0};
    }
}

void Manager::OnContainerMenuEnter()
{
    if (queued_real_to_sendback.first && queued_real_to_sendback.second) {
		real_to_sendback = queued_real_to_sendback;
		const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(real_to_sendback.second);
		const auto unownedChestOG = RE::TESForm::LookupByID<RE::TESObjectREFR>(unownedChestOGRefID);
		RemoveItem(chest, unownedChestOG, real_to_sendback.first, RE::ITEM_REMOVE_REASON::kStoreInContainer);
		const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(ChestToFakeContainer.at(real_to_sendback.second).innerKey);
		fake_bound->formFlags = 13;
    }
	queued_real_to_sendback = { nullptr,0 };
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
        logger::trace("ChestToFakeContainer (chest refid: {:x}) before: {:x}", chest_ref,
                      ChestToFakeContainer[chest_ref].innerKey);
        ChestToFakeContainer[chest_ref].innerKey = fakeid;
        logger::trace("ChestToFakeContainer (chest refid: {:x}) after: {:x}", chest_ref,
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

void Manager::FakePlacementCeption(const RefID chest_ref, std::vector<RefID>& ha) {

    // ha: handled already
    if (std::ranges::find(ha, chest_ref) != ha.end()) return;
    logger::info("-------------------chest_ref: {:x} -------------------", chest_ref);
    for (const auto connected_chests = GetConnectedChests(chest_ref); const auto& connected_chest : connected_chests) {
        logger::info("Connected chest: {:x}", connected_chest);
        FakePlacementCeption(connected_chest,ha);
    }
    if (const auto src = GetContainerSource(ChestToFakeContainer.at(chest_ref).outerKey)) {
        FakePlacement(src->data.at(chest_ref), chest_ref);
        ha.push_back(chest_ref);
        logger::info("-------------------chest_ref: {:x} DONE -------------------", chest_ref);
    }
    else return RaiseMngrErr("Could not find source for container");
        
}

void Manager::FakePlacement(RefID saved_ref, const RefID chest_ref, RE::TESObjectREFR* external_cont) {

    // bu sadece load sirasinda
    // ya playerda olcak ya da unownedlardan birinde (containerception)
    // bu ikisi disindaki seylere load_game safhasinda bisey yapamiyorum external_cont nullptr sa
    if (Settings::is_pre_0_10_0) {
		if (chest_ref == saved_ref) saved_ref = 0x14;
    }
    if (!external_cont && chest_ref != 0x14 && !IsChest(saved_ref)) return;

    // saved_ref should not be realcontainer out in the world!
    if (IsRealContainer(external_cont)) {
        logger::critical("saved_ref should not be realcontainer out in the world!");
        return;
    }

    // external cont mu yoksa ya playerda ya da unownedlardan birinde miyi anliyoruz
    // pre 0.10: //const RefID cont_ref = chest_ref == saved_ref ? 0x14 : saved_ref; // only changing in the case of indication of player has fake container
    const RefID cont_ref = saved_ref;
    const auto cont_of_fakecont = external_cont ? external_cont : RE::TESForm::LookupByID<RE::TESObjectREFR>(cont_ref);
    if (!cont_of_fakecont) return RaiseMngrErr("cont_of_fakecont not found");

    auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;  // dont use this again bcs it can change after qTRICK_
    RE::TESBoundObject* fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);

    if (fake_bound && fake_bound->IsDeleted()) {
        logger::warn("Fake container with formid {:x} is deleted. Removing it from inventory/external_container...",
                     fake_formid);
        RemoveItem(cont_of_fakecont, nullptr, fake_bound, RE::ITEM_REMOVE_REASON::kRemove);
    }

    if (!fake_bound || !HasItemPlusCleanUp(fake_bound, cont_of_fakecont)) {
        qTRICK_(chest_ref, cont_ref, true);
    }
    else {
        if (!std::strlen(fake_bound->GetName())) {
            logger::warn("Fake container found in {} with empty name.", cont_of_fakecont->GetDisplayFullName());
        }
        if (fake_formid != fake_bound->GetFormID()) {
            logger::warn("Fake container formid changed from {} to {}", fake_formid, fake_bound->GetFormID());
        }
        logger::trace("Fake container found in {} with name {} and formid {:x}.",
                      cont_of_fakecont->GetDisplayFullName(), fake_bound->GetName(), fake_bound->GetFormID());
        qTRICK_(chest_ref, cont_ref);
    }
        
    // (pre 0.10) yani playerda deilse
    //if (chest_ref != saved_ref) {
    if (saved_ref != 0x14) {
		const auto fake_bound_2 = RE::TESForm::LookupByID<RE::TESBoundObject>(ChestToFakeContainer[chest_ref].innerKey);
        RemoveItem(player_ref, cont_of_fakecont, fake_bound_2,
                   RE::ITEM_REMOVE_REASON::kStoreInContainer);
    }
}

void Manager::RemoveCarryWeightBoost(const FormID item_formid, RE::TESObjectREFR* inventory_owner) {

    const auto item_obj = RE::TESForm::LookupByID<RE::TESBoundObject>(item_formid);
    if (!item_obj) return RaiseMngrErr("Item not found");
    if (!Inventory::HasItem(item_obj, inventory_owner)) {
        logger::warn("Item not found in player's inventory");
        return;
    }
    const auto inventory = inventory_owner->GetInventory();
    if (const auto enchantment = inventory.find(item_obj)->second.second->GetEnchantment()) {
        for (const auto& effect : enchantment->effects) {
            if (effect->baseEffect->data.primaryAV == RE::ActorValue::kCarryWeight) {
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
		std::shared_lock lock(source_mutex_);
        if (const auto container_refid = a_container->GetFormID(); // register the container to the source data if it is not registered
            !Functions::containsValue(src->data,container_refid)) {
            const auto ChestObjRef = FindNotMatchedChest(); // Not registered. lets find a chest to register it to
            const auto ChestRefID = ChestObjRef->GetFormID();

            logger::info("Matched chest with refid: {:x} with container with refid: {:x}", ChestRefID, container_refid);
            lock.unlock();
            if (std::unique_lock lock2(source_mutex_); !src->data.insert({ChestRefID, container_refid}).second) {
			    logger::error("Failed to insert chest refid {:x} and container refid {:x} into source data.", ChestRefID,
				    container_refid);
			    return false;
            }
            // add to ChestToFakeContainer
			if (const auto fake_formid = CreateFakeContainer(a_container->GetObjectReference(), ChestRefID, nullptr); !fake_formid) {
				return false;
			}
            else if (std::unique_lock lock2(chest2fake_mutex_); !ChestToFakeContainer.insert({ChestRefID, {.outerKey= src->formid, .innerKey= fake_formid}}).second) {
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
            lock.lock();
            if (const auto& initial_items_map = src->initial_items; !initial_items_map.empty()) {
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
            const auto real_cont_id = GetRealID(chest_refid);
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
				std::unique_lock lock2(chest2fake_mutex_);
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

    if (result != 0 && result != 1 && result != 2 && result != 3) return;

    // More
    if (result == 2) {
		SKSE::GetTaskInterface()->AddTask([this]() {
            MsgBoxesNotifs::ShowMessageBox("...", buttons_more, [this](const int res) { this->MsgBoxCallbackMore(res); });
		});
		return;
    }

    if (result == 3 || result == 1){
        const auto chest = GetRealContainerChest(current_container);
        if (!chest) return RaiseMngrErr("MsgBoxCallback Chest not found");
        SendReal(current_container->GetBaseObject(), chest);
        // erase real_formid from vector reals_to_sendback
        real_to_sendback = {nullptr,0};

    }

    // Close
    if (result == 3) {
		current_container = nullptr;
        return;
    }
        
    // Take
    if (result == 1) {
        RE::PlayerCharacter::GetSingleton()->PickUpObject(current_container,1);
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
        const auto chest_rename = renames.contains(fake_id) ? renames.at(fake_id).c_str() : current_container->GetName();
        if (ActivateChest(chest, chest_rename)) {
            real_to_sendback = {current_container->GetBaseObject(), chest_refid};
		}
		else {
			logger::critical("Failed to activate chest.");
			MsgBoxCallback(3);
        }
    }
    else {
		logger::critical("Chest not found.");
    }
}

void Manager::MsgBoxCallbackMore(const int result) {

    if (result != 0 && result != 1 && result != 2 && result != 3) return;

    // Rename
    if (result == 0) {
        if (!uiextensions_is_present) return MsgBoxCallback(3);
        const char* menuID = "UITextEntryMenu";
        const char* property_name = "text";
        const char* container_name =
            RE::TESForm::LookupByID<RE::TESBoundObject>(
                ChestToFakeContainer.at(GetRealContainerChest(current_container)->GetFormID()).innerKey)
            ->GetName();
        if (Papyrus::CallFunction("UIExtensions","SetMenuPropertyString",menuID,property_name,container_name)) {
            const auto skyrimVM = RE::SkyrimVM::GetSingleton();
            if (const auto vm = skyrimVM ? skyrimVM->impl : nullptr) {
                RE::TESForm* emptyForm = nullptr;
                RE::TESForm* emptyForm2 = nullptr;
                const auto args = RE::MakeFunctionArguments(std::move(menuID), std::move(emptyForm),
                                                            std::move(emptyForm2));
                RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new Papyrus::RenameCallbackFunctor());
			    if (!vm->DispatchStaticCall("UIExtensions", "OpenMenu", args, callback)) {
					logger::error("Failed to call UIExtensions OpenMenu.");
					MsgBoxCallback(3);
			    }
		    }
			else {
				logger::error("Failed to get SkyrimVM.");
				MsgBoxCallback(3);
			}
		}
		else {
			logger::error("Failed to call UIExtensions functions.");
			MsgBoxCallback(3);
		}
        return;
    }

    // Close
    if (result == 3) return;

    // Back
    if (result == 2) {
		SKSE::GetTaskInterface()->AddTask([this]() {
            PromptInterface();
		});
        return;
    }

    MsgBoxCallback(3);
    Uninstall();

}

void Manager::PromptInterface() {

    const auto src = GetContainerSource(current_container->GetBaseObject()->GetFormID());
    if (!src) return RaiseMngrErr("Could not find source for container");
        
    const auto chest = GetRealContainerChest(current_container);
    const auto fake_id = ChestToFakeContainer[chest->GetFormID()].innerKey;

    const std::string name = renames.contains(fake_id) ? renames[fake_id] : current_container->GetDisplayFullName();

    // Round the float to 2 decimal places
    std::ostringstream stream1;
    stream1 << std::fixed << std::setprecision(2) << chest->GetWeightInContainer()*src->weight_ratio;
    std::ostringstream stream2;
    stream2 << std::fixed << std::setprecision(2) << src->capacity;

    const auto stream1_str = stream1.str();
    const auto stream2_str = src->capacity > 0 ? "/" + stream2.str() : ""; 

    return MsgBoxesNotifs::ShowMessageBox(
        name + " | W: " + stream1_str + stream2_str + " | V: " + std::to_string(GetChestValue(chest)),
        buttons,
        [this](const int result) { this->MsgBoxCallback(result); });
}

RE::ObjectRefHandle Manager::RemoveItem(RE::TESObjectREFR* moveFrom, RE::TESObjectREFR* moveTo, RE::TESBoundObject* a_item,
                                        const RE::ITEM_REMOVE_REASON reason) {

    auto ref_handle = RE::ObjectRefHandle();

    if (!moveFrom) {
        logger::critical("moveFrom is null!");
        return ref_handle;
    }
    if (moveTo && moveFrom->GetFormID() == moveTo->GetFormID()) {
        logger::info("moveFrom and moveTo are the same!");
        return ref_handle;
    }

	const auto inventory = moveFrom->GetInventory();
	const auto it_item = inventory.find(a_item);
	if (it_item == inventory.end()) {
		logger::warn("Item not found in inventory {:x}", a_item ? a_item->GetFormID() : 0);
		return ref_handle;
	}

    const auto inv_data = it_item->second.second.get();
    if (const auto asd = inv_data ? inv_data->extraLists : nullptr; !asd || asd->empty()) {
        ref_handle = moveFrom->RemoveItem(a_item, 1, reason, nullptr, moveTo);
    } else {
        ref_handle = moveFrom->RemoveItem(a_item, 1, reason, asd->front(), moveTo);
    }
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
            return true;
        } else logger::trace("item count: {}", new_item_count);
        i++;
    }

    return false;
}

bool Manager::MoveObject(RE::TESObjectREFR* ref, RE::TESObjectREFR* move2container, const bool owned) {

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

void Manager::UpdateFakeWV(RE::TESBoundObject* fake_form)
{
	const auto chestID = GetFakeContainerChestID(fake_form->GetFormID());
	if (const auto chestRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(chestID)) {
		if (const auto src = GetContainerSource(ChestToFakeContainer.at(chestID).outerKey)) {
			UpdateFakeWV(fake_form, chestRef, src->weight_ratio);
		}
		else {
			logger::error("Source not found.");
		}
	}
	else {
		logger::error("Chest ref not found.");
	}
}

Count Manager::CanBeAdded(const RE::TESBoundObject* a_item, const Count a_count, const RE::TESBoundObject* fake_container)
{
	if (a_item->GetWeight() < 0.001f) return a_count;

    const auto chestID = GetFakeContainerChestID(fake_container->GetFormID());
	if (const auto chestRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(chestID)) {
		if (const auto src = GetContainerSource(ChestToFakeContainer.at(chestID).outerKey)) {
			if (src->weight_ratio < 0.001f) return a_count;
            const auto remaining_capacity = src->capacity - chestRef->GetWeightInContainer() * src->weight_ratio;
			const auto item_weight = a_item->GetWeight() * src->weight_ratio;
			const auto can_be_added = static_cast<Count>(remaining_capacity / (item_weight+EPSILON));
			return std::max(0,std::min(can_be_added, a_count));
		}
		logger::error("Source not found.");
	}
	else {
		logger::error("Chest ref not found.");
	}
    return 0;
}

bool Manager::UpdateExtrasInInventory(RE::TESObjectREFR* from_inv, const FormID from_item_formid,
                                      RE::TESObjectREFR* to_inv, const FormID to_item_formid) {
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
        const auto to_refhandle = RemoveItem(to_inv, nullptr, to_item, RE::ITEM_REMOVE_REASON::kDropping);
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

    if (!xData::UpdateExtras(extralist_from, extralist_to)) {
        logger::error("Failed to update extras");
        return false;
    }

    if (RE::TESObjectREFR* ref_from = nullptr; removed_from && !MoveObject(ref_from, from_inv)) return false;
    if (removed_to && !MoveObject(ref_to, to_inv)) return false;

    return true;
}

void Manager::HandleFormDelete_(const RefID chest_refid) {

	std::shared_lock lock(chest2fake_mutex_);
    auto real_formid = ChestToFakeContainer[chest_refid].outerKey;
    if (const auto real_item = RE::TESForm::LookupByID<RE::TESBoundObject>(real_formid)) {
        const auto msg =
            std::format("Your container with name {} was deleted by the game. Will try to return your items now.",
                        real_item->GetName());
        MsgBoxesNotifs::InGame::CustomMsg(msg);
    }
    else {
        const auto msg =
            std::format("Your container with formid {:x} was deleted by the game. Will try to return your items now.",
                        real_formid);
        MsgBoxesNotifs::InGame::CustomMsg(msg);
    }
    const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
    const auto fake_formid = ChestToFakeContainer[chest_refid].innerKey;

    lock.unlock();

    if (chest) {
        DeRegisterChest(chest_refid);
    }

    else MsgBoxesNotifs::InGame::CustomMsg("Could not return your items.");
	if (const auto fake_item = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid)) {
        RemoveItem(player_ref, nullptr, fake_item, RE::ITEM_REMOVE_REASON::kRemove);
	}
    else {
		logger::error("Fake item not found.");
    }
	
}

void Manager::Reset() {
    logger::info("Resetting manager...");

	/*std::unique_lock lock(source_mutex_);
	std::unique_lock lock2(chest2fake_mutex_);*/

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
            auto fake_formid = ChestToFakeContainer.at(chest_ref).innerKey;
            if (cont_ref == 0x14) {
                const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
                is_equipped_x = Inventory::IsEquipped(fake_bound);
                is_favorited_x = Inventory::IsFavorited(fake_bound,player_ref);
                if (const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref); !chest) return RaiseMngrErr("Chest not found");
            } 
            // check if the fake container is faved in an external container
            else if (auto it = std::ranges::find(external_favs, fake_formid); 
                it != external_favs.end()) {
                is_favorited_x = true;
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

    std::map<RefID,std::pair<bool,bool>> chest_equipped_fav;

    std::map<RefID, FormFormID> unmathced_chests;
    for (const auto& [realcontForm_chestRef, fakecontForm_contRef] : m_Data) {

        bool no_match = true;
        FormID realcontForm = realcontForm_chestRef.outerKey;
        RefID chestRef = realcontForm_chestRef.innerKey;
        FormIDX fakecontForm = fakecontForm_contRef.outerKey;
        RefID contRef = fakecontForm_contRef.innerKey;
        if (Settings::is_pre_0_10_0 && contRef == chestRef) {
			contRef = 0x14;
        }

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

            if (contRef == 0x14) chest_equipped_fav[chestRef] = {fakecontForm.equipped, fakecontForm.favorited};
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
        FakePlacementCeption(chest_ref,handled_already);
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
        logger::info("{:x}", ref);
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
			const auto fakecontainerchestid = GetFakeContainerChestID(fake_formid);
            const auto [is_equipped_x,is_faved_x ] = chest_equipped_fav[fakecontainerchestid];
            if (is_equipped_x) {
                logger::trace("Equipping fake container with formid {:x}", fake_formid);
                Inventory::EquipItem((*it)->object);
            }
            if (is_faved_x) {
                logger::trace("Favoriting fake container with formid {:x}", fake_formid);
                Inventory::FavoriteItem((*it)->object,player_ref);
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
    Print();
        
    current_container = nullptr;
}