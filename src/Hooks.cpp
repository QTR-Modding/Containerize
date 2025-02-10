#include "Hooks.h"

void Hooks::Install()
{
	MoveItemHooks<RE::PlayerCharacter>::install();
	MoveItemHooks<RE::TESObjectREFR>::install(false);
	MoveItemHooks<RE::Character>::install();
	MenuHook<RE::ContainerMenu>::InstallHook(RE::VTABLE_ContainerMenu[0]);

	auto& trampoline = SKSE::GetTrampoline();
    constexpr size_t size_per_hook = 14;
	trampoline.create(size_per_hook*2);

	const REL::Relocation<std::uintptr_t> target4{REL::RelocationID(67315, 68617)};
    InputHook::func = trampoline.write_call<5>(target4.address() + 0x7B, InputHook::thunk);

	const REL::Relocation<std::uintptr_t> add_item_functor_hook{ RELOCATION_ID(55946, 56490) };
	add_item_functor_ = trampoline.write_call<5>(add_item_functor_hook.address() + 0x15D, add_item_functor);

}
bool Hooks::HandleEquip(RE::InputEvent* event)
{
#undef GetObject
	const auto ui = RE::UI::GetSingleton();
	if (!ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) &&
		!ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)&&
		!ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME)) {
		equip_was_pressed.store(false);
		return false;
	}

	const auto user_events = RE::UserEvents::GetSingleton();
    if (const auto button_event = event->AsButtonEvent()) {
		//logger::info("HandleEquip event. {}",button_event->userEvent);
		if (const auto user_event = button_event->userEvent;
			user_event == user_events->accept ||
			user_event == user_events->leftEquip ||
			user_event == user_events->rightEquip
			) {
			if (button_event->IsDown()) {
				equip_was_pressed.store(true);
			}
			if (!equip_was_pressed.load()) {
				return false;
			}
			if (const auto selected_item = GetSelectedItemInMenu(); 
				selected_item && M->IsFakeContainer(selected_item->GetFormID())) {
				if (button_event->HeldDuration()>0.25f) {
					M->OnLongPressEquip(selected_item);
				}
				else if (button_event->IsUp()) {
					const auto player = RE::PlayerCharacter::GetSingleton();
					if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
						RE::TESObjectREFRPtr refr_container;
						if (LookupReferenceByHandle(RE::ContainerMenu::GetTargetRefHandle(), refr_container)) {
						    const auto container_menu = ui->GetMenu<RE::ContainerMenu>();
						    const auto item_list = container_menu->GetRuntimeData().itemList;
							for (const auto a_item : item_list->items) {
								if (a_item->data.objDesc->GetObject()->GetFormID() == selected_item->GetFormID()) {
                                    if (RE::TESObjectREFRPtr refr_owner; LookupReferenceByHandle(a_item->data.owner,refr_owner)) {
									    if (refr_owner->IsPlayerRef()) {
											if (user_event == user_events->leftEquip || user_event == user_events->rightEquip) {
												Inventory::ToggleEquip(selected_item);
											}
											else {
											    refr_owner->RemoveItem(selected_item,1,RE::ITEM_REMOVE_REASON::kStoreInContainer,nullptr,refr_container.get());
											}
									    }
										else {
										    refr_owner->RemoveItem(selected_item,1,RE::ITEM_REMOVE_REASON::kStoreInContainer,nullptr,player);
											if (user_event == user_events->leftEquip || user_event == user_events->rightEquip) {
											    Inventory::ToggleEquip(selected_item);
											}
										}
									}
									break;
								}
							}
					        RE::SendUIMessage::SendInventoryUpdateMessage(refr_container.get(),nullptr);
						}
					}
					else {
						if (Inventory::IsEquipped(selected_item)) {
							logger::info("Unequipping item.");
							RE::ActorEquipManager::GetSingleton()->UnequipObject(player,selected_item);
						}
						else {
							logger::info("Equipping item.");
						    RE::ActorEquipManager::GetSingleton()->EquipObject(
							    player, selected_item);
						}
					}
					RE::SendUIMessage::SendInventoryUpdateMessage(player,nullptr);
				}
			    return true;
			}
		}
	}
    return false;
}

RE::TESBoundObject* Hooks::GetSelectedItemInMenu()
{
	if (const auto ui = RE::UI::GetSingleton()) {
	    if (const auto menu_c = ui->GetMenu<RE::ContainerMenu>()) {
		    if (const auto item = menu_c->GetRuntimeData().itemList->GetSelectedItem()) {
				return item->data.objDesc->GetObject();
		    }
	    }
	    else if (const auto menu_i = ui->GetMenu<RE::InventoryMenu>()) {
		    if (const auto item = menu_i->GetRuntimeData().itemList->GetSelectedItem()) {
				return item->data.objDesc->GetObject();
		    }
	    }
	    else if (const auto menu_f = ui->GetMenu<RE::FavoritesMenu>()) {
		    RE::GFxValue selectedIndex;
		    const auto& runtime_data = menu_f->GetRuntimeData();
		    if (runtime_data.root.GetMember("selectedIndex", &selectedIndex) && selectedIndex.IsNumber()) {
                const std::int32_t selected_index = static_cast<std::int32_t>(selectedIndex.GetNumber());
			    const auto& items = runtime_data.favorites;
				if (selected_index >= 0 && selected_index < items.size()) {
			        if (const auto item = items[selected_index].item) {
				        if (const auto bound = skyrim_cast<RE::TESBoundObject*>(item)) {
						    return bound;
				        }
			        }
				}
		    }
	    }
	}
	return nullptr;
}

void Hooks::add_item_functor(RE::TESObjectREFR* a_this, RE::TESObjectREFR* a_object, int32_t a_count, bool a4, bool a5)
{
	logger::info("add_item_functor event.");
	if (!a_this || !a_object || a_count>1) {
		return add_item_functor_(a_this, a_object, a_count, a4, a5);
	}
	M->OnPickup(a_this, a_object);
	return add_item_functor_(a_this, a_object, a_count, a4, a5);
}

template<typename RefType>
void Hooks::MoveItemHooks<RefType>::pickUpObject(RefType * a_this, RE::TESObjectREFR * a_object, int32_t a_count, bool a_arg3, bool a_play_sound)
{
	if (!a_this || !a_object || a_count>1) {
		return pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
	}
	M->OnPickup(a_this, a_object);

	pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
}

template<typename RefType>
void Hooks::MoveItemHooks<RefType>::addObjectToContainer(RefType* a_this, RE::TESBoundObject* a_object, RE::ExtraDataList* a_extraList, std::int32_t a_count, RE::TESObjectREFR* a_fromRefr)
{

	if (!a_this || !a_object) {
		return add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);
	}

    /*logger::trace("Object {} {:x} added to {} {:x} from {} {:x}", a_object->GetName(), a_object->GetFormID(),
		a_this->GetName(), a_this->GetFormID(), a_fromRefr->GetName(), a_fromRefr->GetFormID());*/

	const auto original_count = a_count;
	if (const auto chest_id = a_this->GetFormID(); M->IsChest(chest_id)) {
		const auto fake_bound = M->GetFakeBound(chest_id);
		if (const RE::TESBoundObject* real_bound = M->FakeToRealContainer(fake_bound->GetFormID()); real_bound->GetFormID() != a_object->GetFormID()) {
		    a_count = M->CanBeAdded(a_object, a_count, fake_bound);
		}
	}
	if (a_count <= 0) {
		if (a_fromRefr) {
		    a_fromRefr->AddObjectToContainer(a_object,a_extraList,original_count,a_this);
		}
        return;
	}

	if (const auto chest_id = M->GetFakeContainerChestID(a_object->GetFormID())) {
		M->UpdateData(chest_id,a_this->GetFormID());
	}


    return add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);
}

template<typename RefType>
RE::ObjectRefHandle * Hooks::MoveItemHooks<RefType>::RemoveItem(RefType * a_this, RE::ObjectRefHandle & a_hidden_return_argument, RE::TESBoundObject * a_item, std::int32_t a_count, RE::ITEM_REMOVE_REASON a_reason, RE::ExtraDataList * a_extra_list, RE::TESObjectREFR * a_move_to_ref, const RE::NiPoint3 * a_drop_loc, const RE::NiPoint3 * a_rotate)
{
	if (!a_this || !a_item || a_count > 1 || !a_item->IsDynamicForm()) {
		return remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
	}

	if (a_reason == RE::ITEM_REMOVE_REASON::kDropping) {
		RE::ObjectRefHandle* res = remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
		if (res && res->get()) {
			M->HandleDrop(res->get().get());
		}
		return res;
	}
	if (!a_move_to_ref) {
        if (const auto a_formid = a_item->GetFormID(); M->IsFakeContainer(a_formid)) {
			logger::info("Item removed from {} to nowhere for reason {}", a_this->GetName(), static_cast<int>(a_reason));
			M->OnConsume(a_formid);
	    }
	}

	return remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
}

void Hooks::InputHook::thunk(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent* const* a_event)
{
	if (!a_dispatcher || !a_event) {
		return func(a_dispatcher, a_event);
	}

	if (IsOtherButtonHeld(a_event)) {
		return func(a_dispatcher, a_event);
	}

    if (RE::PlayerCharacter::GetSingleton()->IsGrabbing()) {
		return func(a_dispatcher, a_event);
	}

	if (const auto ui = RE::UI::GetSingleton()) {
	    if (ui->IsMenuOpen(RE::BarterMenu::MENU_NAME) ||
		    ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) {
		    return func(a_dispatcher, a_event);
	    }
	}

    auto first = *a_event;
    auto last = *a_event;
    size_t length = 0;

    for (auto current = *a_event; current; current = current->next) {
        if (ProcessInput(current)) {
            if (current != last) {
                last->next = current->next;
            } else {
                last = current->next;
                first = current->next;
            }
        } else {
            last = current;
            ++length;
        }
    }

    if (length == 0) {
        constexpr RE::InputEvent* const dummy[] = {nullptr};
        func(a_dispatcher, dummy);
    } else {
        RE::InputEvent* const e[] = {first};
        func(a_dispatcher, e);
    }
}

bool Hooks::InputHook::ProcessInput(RE::InputEvent* event)
{
	bool block = false;
    if (const auto button_event = event->AsButtonEvent()) {
        if (button_event->userEvent == RE::UserEvents::GetSingleton()->activate) {
            const auto crosshair_pick_data = RE::CrosshairPickData::GetSingleton();
			if (const auto crosshair_target = crosshair_pick_data->target) {
                if (M->IsRealContainer(crosshair_target.get()->GetBaseObject()->GetFormID())) {
		            if (button_event->IsDown()) {
			            block = true;
                    }
                    else if (button_event->IsUp()) {
			            block = true;
                        if (const auto grabbed_ref = RE::PlayerCharacter::GetSingleton()->GetGrabbedRef(); 
                            !grabbed_ref || crosshair_target.get()->GetFormID() != grabbed_ref->GetFormID()) {
                            M->OnActivateContainer(crosshair_target.get().get());
                        }
                    }
				}
			}
		}
    }
	return block ? block : HandleEquip(event);
}

bool Hooks::InputHook::IsOtherButtonHeld(RE::InputEvent* const* a_event) {
	for (auto current = *a_event; current; current = current->next) {
		if (const auto button_event = current->AsButtonEvent()) {
			if (button_event->IsHeld()) {
				return true;
			}
		}
	}
    return false;
}

template<typename MenuType>
RE::UI_MESSAGE_RESULTS Hooks::MenuHook<MenuType>::ProcessMessage_Hook(RE::UIMessage& a_message)
{
	const auto msg_type = static_cast<int>(a_message.type.get());
	if (msg_type != 3 && msg_type != 1) {
		return _ProcessMessage(this, a_message);
	}
	if (const std::string_view menuname = MenuType::MENU_NAME; a_message.menu==menuname) {
	    if (menuname == RE::ContainerMenu::MENU_NAME) {
            if (RE::TESObjectREFRPtr refr; LookupReferenceByHandle(RE::ContainerMenu::GetTargetRefHandle(), refr)) {
				if (M->IsChest(refr->GetFormID())) {
			        if (msg_type == 3) M->OnContainerMenuExit();
			        else if (msg_type == 1) M->OnContainerMenuEnter();
				}
			}
        }
	}
    return _ProcessMessage(this, a_message);
}

template<typename MenuType>
void Hooks::MenuHook<MenuType>::InstallHook(const REL::VariantID& varID)
{
    REL::Relocation<std::uintptr_t> vTable(varID);
    _ProcessMessage = vTable.write_vfunc(0x4, &MenuHook<MenuType>::ProcessMessage_Hook);
}