#include "Events.h"

RE::BSEventNotifyControl OurEventSink::OnRename() const {
    logger::trace("Rename menu closed.");
    M->listen_menu_close.store(false);
    const auto skyrimVM = RE::SkyrimVM::GetSingleton();
    const auto vm = skyrimVM ? skyrimVM->impl : nullptr;
    if (!vm) return RE::BSEventNotifyControl::kContinue;
    const char* menuID = "UITextEntryMenu";
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new ConversationCallbackFunctor(M));
    const auto args = RE::MakeFunctionArguments(std::move(menuID));
    vm->DispatchStaticCall("UIExtensions", "GetMenuResultString", args, callback);
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl OurEventSink::ToggleEquipOpenContainer()
{
    equipped = false;
    logger::trace("Reverting equip...");
	const auto fake_id = fake_id_;
	SKSE::GetTaskInterface()->AddTask([fake_id]() { Manager::RevertEquip(fake_id); });
    //Manager::RevertEquip(fake_id_);
    logger::trace("Reverted equip.");
    M->ActivateContainer(fake_id_, true);
    return RE::BSEventNotifyControl::kContinue;
}

void OurEventSink::ReShow()
{
    M->UnHideReal(fake_id_);
    if (ReShowMenu == RE::ContainerMenu::MENU_NAME && !external_container_refid) {
        logger::warn("External container refid is 0.");
    }
    if (ReShowMenu != RE::ContainerMenu::MENU_NAME && external_container_refid) {
        logger::warn("ReShowMenu is not ContainerMenu.");
    }
    /*if (external_container_refid && ReShowMenu == RE::ContainerMenu::MENU_NAME) {
        M->RevertEquip(fake_id_, external_container_refid);
    }*/
    if (other_settings[Settings::otherstuffKeys[2]]) {
        logger::trace("Returning to initial menu: {}", ReShowMenu);
        if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
            if (external_container_refid && ReShowMenu == RE::ContainerMenu::MENU_NAME) {
                if (const auto a_objref = RE::TESForm::LookupByID<RE::TESObjectREFR>(external_container_refid)) {
                    const auto player_ref = RE::PlayerCharacter::GetSingleton();
                    if (a_objref->HasContainer()) {
                        if (a_objref->As<RE::TESObjectCONT>()) {
                            a_objref->OpenContainer(0);
                        } 
                        else if (a_objref->GetBaseObject()->As<RE::TESObjectCONT>()) {
                            a_objref->OpenContainer(0);
                        } 
                        else {
                            logger::trace("has container but could not activate.");
                            a_objref->OpenContainer(3);
                        }
                    } else a_objref->ActivateRef(player_ref, 0, a_objref->GetBaseObject(), 1, false);
                }
            } 
            else queue->AddMessage(ReShowMenu, RE::UI_MESSAGE_TYPE::kShow, nullptr);
        }
        else logger::warn("Failed to return to initial menu.");
    }
    external_container_refid = 0;
    ReShowMenu = "";
}

bool OurEventSink::HideMenuOnEquipHeld()
{
    if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
        if (const auto ui = RE::UI::GetSingleton(); ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
            ReShowMenu = RE::InventoryMenu::MENU_NAME;
            queue->AddMessage(RE::TweenMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
        } 
        else if (ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME)) {
            ReShowMenu = RE::FavoritesMenu::MENU_NAME;
        }
        else if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
            ReShowMenu = RE::ContainerMenu::MENU_NAME;
            const auto menu = ui ? ui->GetMenu<RE::ContainerMenu>() : nullptr;
            const RefID refHandle = menu ? menu->GetTargetRefHandle() : 0;
            RE::TESObjectREFRPtr ref;
            RE::LookupReferenceByHandle(refHandle, ref);
            if (ref) external_container_refid = ref->GetFormID();
            else logger::warn("Failed to get ref from handle.");
            logger::trace("External container refid: {}", external_container_refid);
        }
        else return false;
        queue->AddMessage(ReShowMenu, RE::UI_MESSAGE_TYPE::kHide, nullptr);
        return true;
    }
	return false;
}

void OurEventSink::Reset() {
	equipped = false;
	fake_equipped_id = 0;
	fake_id_ = 0;
	ReShowMenu = "";
	furniture = nullptr;
	furniture_entered.store(false);
	listen_crosshair_ref.store(true);
	listen_weight_limit.store(false);
	block_droptake.store(false);
	block_eventsinks.store(false);
	external_container_refid = 0;
}


RE::BSEventNotifyControl OurEventSink::ProcessEvent(const RE::TESEquipEvent* event,
    RE::BSTEventSource<RE::TESEquipEvent>*) {
    if (block_eventsinks.load()) return RE::BSEventNotifyControl::kContinue;
    if (!event) return RE::BSEventNotifyControl::kContinue;
    if (!event->actor->IsPlayerRef()) return RE::BSEventNotifyControl::kContinue;
        
    if (!M->IsFakeContainer(event->baseObject)) return RE::BSEventNotifyControl::kContinue;

    fake_equipped_id = event->equipped ? event->baseObject : 0;

	if (!ReShowMenu.empty()) return RE::BSEventNotifyControl::kContinue; // set in HideMenuOnEquipHeld

    if (const auto ui = RE::UI::GetSingleton(); !(ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) || 
                                                    ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME) || 
                                                    ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME))) {
        return RE::BSEventNotifyControl::kContinue;
    }

    fake_id_ = event->baseObject;
    equipped = true;

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl OurEventSink::ProcessEvent(const RE::TESActivateEvent* event,
    RE::BSTEventSource<RE::TESActivateEvent>*) {
        
    if (block_eventsinks.load()) return RE::BSEventNotifyControl::kContinue;
    if (!M->listen_activate.load()) return RE::BSEventNotifyControl::kContinue;
    if (!event) return RE::BSEventNotifyControl::kContinue;
    if (!event->objectActivated) return RE::BSEventNotifyControl::kContinue;
    if (!event->actionRef->IsPlayerRef()) return RE::BSEventNotifyControl::kContinue;
    if (!event->objectActivated->IsActivationBlocked()) return RE::BSEventNotifyControl::kContinue;
    if (event->objectActivated == RE::PlayerCharacter::GetSingleton()->GetGrabbedRef()) return RE::BSEventNotifyControl::kContinue;
    if (!M->IsRealContainer(event->objectActivated.get())) return RE::BSEventNotifyControl::kContinue;

    //bool skip_interface = false;
    //if (po3_use_or_take) {
    //    if (const auto base = event->objectActivated->GetBaseObject()) {
    //        RE::BSString str;
    //        base->GetActivateText(RE::PlayerCharacter::GetSingleton(), str);
    //        if (String::includesWord(str.c_str(), {"Equip", "Eat", "Drink"})) skip_interface = true;
    //    }
    //}
        
    logger::trace("Container activated");
    /*M->OnActivateContainer(event->objectActivated.get(),skip_interface);*/

#ifndef NDEBUG
    M->Print();
#endif


    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl OurEventSink::ProcessEvent(const SKSE::CrosshairRefEvent* event,
    RE::BSTEventSource<SKSE::CrosshairRefEvent>*) {

    if (block_eventsinks.load()) return RE::BSEventNotifyControl::kContinue;
    if (!event->crosshairRef) return RE::BSEventNotifyControl::kContinue;
    if (event->crosshairRef->extraList.GetCount()>1) return RE::BSEventNotifyControl::kContinue;
    if (!listen_crosshair_ref) return RE::BSEventNotifyControl::kContinue;

    if (const auto crosshair_refr = event->crosshairRef.get(); !M->IsRealContainer(crosshair_refr)) {
        // if the fake items are not in it we need to place them (this happens upon load game)
        listen_crosshair_ref = false;
        M->HandleFakePlacement(crosshair_refr);
        listen_crosshair_ref = true;
        return RE::BSEventNotifyControl::kContinue;
        
    }

    //if (event->crosshairRef->IsActivationBlocked() && !M->isUninstalled.load()) return RE::BSEventNotifyControl::kContinue;

    //    
    //if (M->isUninstalled.load()) {
    //    event->crosshairRef->SetActivationBlocked(false);
    //} else {
    //    event->crosshairRef->SetActivationBlocked(true);
    //}
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl OurEventSink::ProcessEvent(const RE::MenuOpenCloseEvent* event,
    RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {

        
  //  if (!event) return RE::BSEventNotifyControl::kContinue;
  //  if (const auto ui = RE::UI::GetSingleton(); event->menuName == "CustomMenu" && 
  //      !ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) &&
  //      !ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) &&
  //      !ui->IsMenuOpen(RE::BarterMenu::MENU_NAME) &&
  //      !ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME) &&
  //      !event->opening && M->listen_menu_close.load()) {
		//return OnRename();
  //  }

  //  if (equipped && event->menuName.c_str() == ReShowMenu && !event->opening) {
  //      logger::trace("menu closed: {}", event->menuName.c_str());
		//return ToggleEquipOpenContainer();
  //  }

  //  if (!M->listen_menu_close.load()) return RE::BSEventNotifyControl::kContinue;

  //  if (event->menuName != RE::ContainerMenu::MENU_NAME) return RE::BSEventNotifyControl::kContinue;

  //  if (event->opening) listen_weight_limit = true;
  //  else {
  //      //logger::trace("Our Container menu closed.");
  //      listen_weight_limit.store(false);
  //      listen_menu_close.store(false);
  //      //logger::trace("listen_menuclose: {}", M->listen_menu_close.load());
  //      if (!ReShowMenu.empty()) ReShow();
  //      else M->HandleContainerMenuExit();
  //  }

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl OurEventSink::ProcessEvent(const RE::TESFurnitureEvent* event,
    RE::BSTEventSource<RE::TESFurnitureEvent>*) {
        
    if (block_eventsinks.load()) return RE::BSEventNotifyControl::kContinue;
    if (!event) return RE::BSEventNotifyControl::kContinue;
    if (!event->actor->IsPlayerRef()) return RE::BSEventNotifyControl::kContinue;
    if (furniture_entered && event->type == RE::TESFurnitureEvent::FurnitureEventType::kEnter)
        return RE::BSEventNotifyControl::kContinue;
    if (!furniture_entered && event->type == RE::TESFurnitureEvent::FurnitureEventType::kExit)
        return RE::BSEventNotifyControl::kContinue;
    if (event->targetFurniture->GetBaseObject()->formType.underlying() != 40) return RE::BSEventNotifyControl::kContinue;

    logger::trace("Furniture event");

    const auto bench = event->targetFurniture->GetBaseObject()->As<RE::TESFurniture>();
    if (!bench) return RE::BSEventNotifyControl::kContinue;
    if (const auto bench_type = static_cast<std::uint8_t>(bench->workBenchData.benchType.get()); bench_type != 2 && bench_type != 3 && bench_type != 7) return RE::BSEventNotifyControl::kContinue;

    if (event->type == RE::TESFurnitureEvent::FurnitureEventType::kEnter) {
        logger::trace("Furniture event: Enter {}", event->targetFurniture->GetName());
        furniture_entered = true;
        furniture = event->targetFurniture;
    }
    else if (event->type == RE::TESFurnitureEvent::FurnitureEventType::kExit) {
        logger::trace("Furniture event: Exit {}", event->targetFurniture->GetName());
        if (event->targetFurniture == furniture) {
            M->HandleCraftingExit();
            furniture_entered = false;
            furniture = nullptr;
        }
    }
    else {
        logger::trace("Furniture event: Unknown");
    }


    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl OurEventSink::ProcessEvent(const RE::TESContainerChangedEvent* event,
                                                    RE::BSTEventSource<RE::TESContainerChangedEvent>*) {
        
    if (block_eventsinks.load()) return RE::BSEventNotifyControl::kContinue;
    if (!M->listen_container_change.load()) return RE::BSEventNotifyControl::kContinue;
    if (!event) return RE::BSEventNotifyControl::kContinue;
    if (!listen_crosshair_ref) return RE::BSEventNotifyControl::kContinue;
    if (furniture_entered) return RE::BSEventNotifyControl::kContinue;
    if (!event->itemCount) return RE::BSEventNotifyControl::kContinue;
    if (event->oldContainer != 20 && event->newContainer != 20) return RE::BSEventNotifyControl::kContinue;

    //// to player inventory <-
    //if (event->newContainer == 20) {
    //    if (event->itemCount == 1 && M->IsRealContainer(event->baseObj) &&
    //        M->RealContainerHasRegistry(event->baseObj)) {
    //        RE::NiPointer<RE::TESObjectREFR> ref_ptr;
    //        if (RE::BSPointerHandleManagerInterface<RE::TESObjectREFR>::GetSmartPointer(event->reference,ref_ptr);
    //            !block_droptake.load() && 
    //            ref_ptr.get() && 
    //            M->IsARegistry(ref_ptr->GetFormID())) {
    //            // somehow, including ref=0 bcs that happens sometimes when NPCs give you your dropped items back...
    //            logger::info("Item {} went into player inventory from unknown container.", event->baseObj);
    //            M->DropTake(event->baseObj, ref_ptr->GetFormID());
    //            M->Print();
    //        }
    //    } else if (M->IsFakeContainer(event->baseObj) && M->ExternalContainerIsRegistered(event->baseObj,event->oldContainer)) {
    //        logger::trace("Unlinking external container.");
    //        M->UnLinkExternalContainer(event->baseObj, event->oldContainer);
    //        M->Print();
    //    }
    //}


    // from player inventory ->
    if (event->oldContainer == 20) {
        // a fake container left player inventory
        if (M->IsFakeContainer(event->baseObj)) {
            logger::trace("Fake container left player inventory.");
            // drop event
            if (!event->newContainer) {
                auto swapped = false;
                logger::trace("Dropped fake container.");
                RE::TESObjectREFR* ref =
                    RE::TESForm::LookupByID<RE::TESObjectREFR>(event->reference.native_handle());
                if (ref) logger::trace("Dropped ref name: {}", ref->GetBaseObject()->GetName());
                if (!ref || ref->GetBaseObject()->GetFormID() != event->baseObj) {
                    // iterate through all objects in the cell................
                    logger::info("Iterating through all references in the cell.");
                    const auto player_cell = RE::PlayerCharacter::GetSingleton()->GetParentCell();
                    auto& cell_runtime_data = player_cell->GetRuntimeData();
                    RE::BSSpinLockGuard locker(cell_runtime_data.spinLock);
                    for (auto& ref_ : cell_runtime_data.references) {
                        if (!ref_) continue;
                        if (ref_.get()->GetBaseObject()->GetFormID()==event->baseObj) {
                            logger::trace("Dropped fake container with ref id {}.", ref_->GetFormID());
                            if (!M->SwapDroppedFakeContainer(ref_.get())) {
                                logger::error("Failed to swap fake container.");
                                return RE::BSEventNotifyControl::kContinue;
                            } 
                            logger::trace("Swapped fake container.");
                            swapped = true;
                            break;
                        }
                    }
                } 
                else if (!M->SwapDroppedFakeContainer(ref)) {
                    logger::error("Failed to swap fake container.");
                    return RE::BSEventNotifyControl::kContinue;
                } 
                else {
                    logger::trace("Swapped fake container.");
                    swapped = true;
                    M->Print();
                }
                if (swapped && obj_manipu_installed && other_settings[Settings::otherstuffKeys[5]]) {
                    WorldObject::StartDraggingObject(ref);
                }
                // consumed
                if (!swapped && event->baseObj==fake_equipped_id) {
                    logger::trace("new container: {}", event->newContainer);
                    logger::trace("old container: {}", event->oldContainer);
                    logger::trace("Sending to handle consume.");
                    fake_equipped_id = 0;
                    equipped = false;
                    M->HandleConsume(event->baseObj);
                }
            }
            // Barter transfer
            else if (RE::UI::GetSingleton()->IsMenuOpen(RE::BarterMenu::MENU_NAME) && Manager::IsCONT(event->newContainer)) {
                logger::info("Sold container.");
                block_droptake = true;
                M->HandleSell(event->baseObj, event->newContainer);
                block_droptake = false;
                M->Print();
            }
            // container transfer
            else if (RE::UI::GetSingleton()->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
                logger::trace("Container transfer.");
                // need to register the container: chestrefid -> thiscontainerrefid
                logger::trace("Container menu is open.");
                M->LinkExternalContainer(event->baseObj, event->newContainer);
                M->Print();
            }
            else {
                MsgBoxesNotifs::InGame::CustomMsg("Unsupported behaviour. Please put back the container you removed from your inventory.");
                logger::error("Unsupported. Please put back the container you removed from your inventory.");
            }
        }
        // check if container has enough capacity
        else if (M->IsChest(event->newContainer) && listen_weight_limit) M->InspectItemTransfer(event->newContainer, event->baseObj);
    }



    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl OurEventSink::ProcessEvent(RE::InputEvent* const* evns, RE::BSTEventSource<RE::InputEvent*>*) {
    if (block_eventsinks.load()) return RE::BSEventNotifyControl::kContinue;
    if (!*evns) return RE::BSEventNotifyControl::kContinue;
    if (!equipped) return RE::BSEventNotifyControl::kContinue; // this also ensures that we have only the menus we want

    for (RE::InputEvent* e = *evns; e; e = e->next) {
        if (e->eventType.get() == RE::INPUT_EVENT_TYPE::kButton) {
            const RE::ButtonEvent* a_event = e->AsButtonEvent();
            const RE::IDEvent* id_event = e->AsIDEvent();
            auto& userEvent = id_event->userEvent;

            if (a_event->IsPressed()) {
                if (a_event->IsRepeating() && a_event->HeldDuration() > .3f && a_event->HeldDuration() < .32f){
                    logger::trace("User event: {}", userEvent.c_str());
                    // we accept : "accept","RightEquip", "LeftEquip", "equip", "toggleFavorite"
                    if (const auto userevents = RE::UserEvents::GetSingleton(); !(userEvent == userevents->accept || userEvent == userevents->rightEquip ||
                        userEvent == userevents->leftEquip || userEvent == userevents->equip)) {
                        continue;
                    }
					if (HideMenuOnEquipHeld()) {
						logger::trace("Menu hidden.");
						return RE::BSEventNotifyControl::kContinue;
					}
                }
            } else if (a_event->IsUp()) equipped = false;
        }
    }
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl OurEventSink::ProcessEvent(const RE::TESFormDeleteEvent* a_event,
    RE::BSTEventSource<RE::TESFormDeleteEvent>*) {
    if (!a_event) return RE::BSEventNotifyControl::kContinue;
    if (!a_event->formID) return RE::BSEventNotifyControl::kContinue;
    M->HandleFormDelete(a_event->formID);
    return RE::BSEventNotifyControl::kContinue;
}