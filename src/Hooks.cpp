#include "Hooks.h"
#include "Manager.h"
#include "SkyPrompt.h"
#include "CLibUtilsQTR/Tasker.hpp"

void Hooks::Install() {
    MoveItemHooks<RE::PlayerCharacter>::install();
    MoveItemHooks<RE::TESObjectREFR>::install(false);
    MoveItemHooks<RE::Character>::install();

    MenuHook<RE::ContainerMenu>::InstallHook(RE::VTABLE_ContainerMenu[0]);
    MenuHook<RE::InventoryMenu>::InstallHook(RE::VTABLE_InventoryMenu[0]);

    auto& trampoline = SKSE::GetTrampoline();
    constexpr size_t size_per_hook = 14;
    constexpr size_t NUM_TRAMPOLINE_HOOKS = 3;
    trampoline.create(size_per_hook * NUM_TRAMPOLINE_HOOKS);

    const REL::Relocation<std::uintptr_t> target4{REL::RelocationID(67315, 68617)};
    InputHook::func = trampoline.write_call<5>(target4.address() + 0x7B, InputHook::thunk);

    const REL::Relocation<std::uintptr_t> add_item_functor_hook{RELOCATION_ID(55946, 56490)};
    add_item_functor_ = trampoline.write_call<5>(add_item_functor_hook.address() + 0x15D, add_item_functor);

    const REL::Relocation<std::uintptr_t> function{REL::RelocationID(51019, 51897)};
    InventoryHoverHook::originalFunction = trampoline.write_call<5>(function.address() + REL::Relocate(0x114, 0x22c),
                                                                    InventoryHoverHook::thunk);
}

void Hooks::InstallUseOrTakeHooks() {
    ActivateHook<RE::TESObjectARMO>::install();
    ActivateHook<RE::TESObjectWEAP>::install();
    ActivateHook<RE::ScrollItem>::install();
    ActivateHook<RE::AlchemyItem>::install();
    ActivateHook<RE::IngredientItem>::install();
}

bool Hooks::HandleEquip(RE::InputEvent* event) {
    const auto user_events = RE::UserEvents::GetSingleton();
    if (const auto button_event = event->AsButtonEvent()) {
        if (const auto& user_event = button_event->GetUserEvent();
            user_event == user_events->accept || user_event == user_events->leftEquip || user_event == user_events->
            rightEquip) {
            if (const auto selected_item = GetSelectedItemInMenu();
                selected_item && Manager::GetSingleton()->IsFakeContainer(selected_item->GetFormID())) {
                if (button_event->IsDown()) {
                    equip_was_pressed.store(true);
                }
                if (!equip_was_pressed.load()) {
                    return false;
                }
                if (button_event->HeldDuration() > 0.25f) {
                    Manager::GetSingleton()->CloseMenu();
                    Manager::GetSingleton()->OnLongPressEquip(selected_item);
                } else if (button_event->IsUp()) {
                    const auto player = RE::PlayerCharacter::GetSingleton();
                    Inventory::ToggleEquip(selected_item);
                    RE::SendUIMessage::SendInventoryUpdateMessage(player, nullptr);
                }
                return true;
            }
            equip_was_pressed.store(false);
        }
    }
    return false;
}

RE::InventoryEntryData* Hooks::GetSelectedEntryInMenu() {
    if (const auto ui = RE::UI::GetSingleton()) {
        if (const auto menu_c = ui->GetMenu<RE::ContainerMenu>()) {
            if (const auto a_itemList = menu_c->GetRuntimeData().itemList) {
                if (const auto item = a_itemList->GetSelectedItem()) {
                    return item->data.objDesc;
                }
            }
        } else if (const auto menu_i = ui->GetMenu<RE::InventoryMenu>()) {
            if (const auto a_itemList = menu_i->GetRuntimeData().itemList) {
                if (const auto item = a_itemList->GetSelectedItem()) {
                    return item->data.objDesc;
                }
            }
        } else if (const auto menu_f = ui->GetMenu<RE::FavoritesMenu>()) {
            RE::GFxValue selectedIndex;
            const auto& runtime_data = menu_f->GetRuntimeData();
            if (runtime_data.root.GetMember("selectedIndex", &selectedIndex) && selectedIndex.IsNumber()) {
                const std::int32_t selected_index = static_cast<std::int32_t>(selectedIndex.GetNumber());
                const auto& items = runtime_data.favorites;
                if (selected_index >= 0 && static_cast<uint32_t>(selected_index) < items.size()) {
                    return items[selected_index].entryData;
                }
            }
        }
    }
    return nullptr;
}

RE::TESBoundObject* Hooks::GetSelectedItemInMenu() {
    #undef GetObject
    if (const auto selected_entry = GetSelectedEntryInMenu()) {
        if (const auto selected_object = selected_entry->GetObject()) {
            return selected_object;
        }
    }
    return nullptr;
}


void Hooks::add_item_functor(RE::TESObjectREFR* a_this, RE::TESObjectREFR* a_object, int32_t a_count, bool a4,
                             bool a5) {
    if (Manager::GetSingleton()->isUninstalled) {
        return add_item_functor_(a_this, a_object, a_count, a4, a5);
    }

    if (!a_this || !a_object || a_count > 1) {
        return add_item_functor_(a_this, a_object, a_count, a4, a5);
    }
    Manager::GetSingleton()->BeforePickup(a_this, a_object);
    return add_item_functor_(a_this, a_object, a_count, a4, a5);
}

template <typename RefType>
void Hooks::MoveItemHooks<RefType>::pickUpObject(RefType* a_this, RE::TESObjectREFR* a_object, int32_t a_count,
                                                 bool a_arg3, bool a_play_sound) {
    if (Manager::GetSingleton()->isUninstalled) {
        return pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
    }

    if (!a_this || !a_object || a_count > 1) {
        return pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
    }
    Manager::GetSingleton()->BeforePickup(a_this, a_object);

    pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
}

template <typename RefType>
void Hooks::MoveItemHooks<RefType>::addObjectToContainer(RefType* a_this, RE::TESBoundObject* a_object,
                                                         RE::ExtraDataList* a_extraList, std::int32_t a_count,
                                                         RE::TESObjectREFR* a_fromRefr) {
    if (Manager::GetSingleton()->isUninstalled) {
        return add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);
    }

    if (!a_this || !a_object || a_count <= 0) {
        return add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);
    }
    const auto original_count = a_count;

    auto M = Manager::GetSingleton();
    if (const auto chest_id = a_this->GetFormID(); M->IsChest(chest_id)) {
        a_count = std::max(0, M->CanBeAdded(a_object, a_count, chest_id));
    }
    if (a_fromRefr && a_count < original_count) {
        a_fromRefr->AddObjectToContainer(a_object, a_extraList, original_count - a_count, a_this);
        if (a_count == 0) {
            return;
        }
    }

    add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);

    if (const auto a_objectID = a_object->GetFormID(); M->IsFakeContainer(a_objectID)) {
        M->UpdateLoc(a_objectID, a_this->GetFormID());
    }
}

template <typename RefType>
RE::ObjectRefHandle* Hooks::MoveItemHooks<RefType>::RemoveItem(RefType* a_this,
                                                               RE::ObjectRefHandle& a_hidden_return_argument,
                                                               RE::TESBoundObject* a_item, std::int32_t a_count,
                                                               RE::ITEM_REMOVE_REASON a_reason,
                                                               RE::ExtraDataList* a_extra_list,
                                                               RE::TESObjectREFR* a_move_to_ref,
                                                               const RE::NiPoint3* a_drop_loc,
                                                               const RE::NiPoint3* a_rotate) {
    auto M = Manager::GetSingleton();

    if (M->isUninstalled || !a_this || !a_item || a_count > 1 || !a_item->IsDynamicForm()) {
        return remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list, a_move_to_ref,
                            a_drop_loc, a_rotate);
    }

    if (a_reason == RE::ITEM_REMOVE_REASON::kDropping) {
        RE::ObjectRefHandle* res = remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason,
                                                a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
        if (res && res->get()) {
            M->HandleDrop(res->get().get());
        }
        return res;
    }

    if (const auto a_id = a_item->GetFormID();
        a_reason == RE::ITEM_REMOVE_REASON::kSelling && M->IsFakeContainer(a_id)) {
        RE::ObjectRefHandle* res = remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason,
                                                a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
        M->UpdateLoc(a_id, a_move_to_ref->GetFormID());
        M->HandleSell(a_id, a_move_to_ref);
        return res;
    }

    if (!a_move_to_ref) {
        if (const auto a_formid = a_item->GetFormID(); M->IsFakeContainer(a_formid)) {
            if (!ModCompatibility::Mods::doppelgangers.contains(a_this->GetBaseObject()->GetFormID())) {
            }
            RE::ObjectRefHandle* res = remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason,
                                                    a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
            M->OnConsume(a_formid, a_this);
            return res;
        }
    }

    return remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list, a_move_to_ref,
                        a_drop_loc, a_rotate);
}

void Hooks::InputHook::thunk(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent* const* a_event) {
    if (Manager::GetSingleton()->isUninstalled) {
        return func(a_dispatcher, a_event);
    }

    if (!a_dispatcher || !a_event) {
        return func(a_dispatcher, a_event);
    }

    if (const auto ui = RE::UI::GetSingleton()) {
        if (!ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME)) {
            return func(a_dispatcher, a_event);
        }
    }

    if (IsOtherButtonHeld(a_event)) {
        return func(a_dispatcher, a_event);
    }

    if (RE::PlayerCharacter::GetSingleton()->IsGrabbing()) {
        return func(a_dispatcher, a_event);
    }

    auto first = *a_event;
    auto last = *a_event;
    size_t length = 0;

    for (auto current = *a_event; current; current = current->next) {
        if (HandleEquip(current)) {
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

template <typename MenuType>
RE::UI_MESSAGE_RESULTS Hooks::MenuHook<MenuType>::ProcessMessage_Hook(RE::UIMessage& a_message) {
    const auto manager = Manager::GetSingleton();
    if (manager->isUninstalled) {
        return _ProcessMessage(this, a_message);
    }

    const auto msg_type = static_cast<int>(a_message.type.get());
    if (msg_type != 3 && msg_type != 1) {
        return _ProcessMessage(this, a_message);
    }

    if (msg_type == 1) {
        is_open.store(false);
        inventory_loaded.store(false);
        if (!Menu::IsPickpocketingOrStealing()) {
            is_open.store(true);
            clib_utilsQTR::Tasker::GetSingleton()->PushTask(
                [] {
                    inventory_loaded.store(true);
                }, inventory_load_time
                );
        }
    } else {
        is_open.store(false);
    }

    SkyPrompt::MenuPromptSink::GetSingleton()->Hide();

    if (const std::string_view menuname = MenuType::MENU_NAME; a_message.menu == menuname) {
        if (menuname == RE::ContainerMenu::MENU_NAME) {
            if (RE::TESObjectREFRPtr refr; LookupReferenceByHandle(RE::ContainerMenu::GetTargetRefHandle(), refr)) {
                if (manager->IsChest(refr->GetFormID())) {
                    if (msg_type == 3) manager->OnChestExit(refr.get());
                    else if (msg_type == 1) manager->OnChestEnter(refr.get());
                }
            }
        }
    }
    return _ProcessMessage(this, a_message);
}

template <typename FormType>
bool Hooks::ActivateHook<FormType>::Activate_Hook(RE::TESBoundObject* a_this, RE::TESObjectREFR* a_targetRef,
                                                  RE::TESObjectREFR* a_activatorRef, std::uint8_t a_arg3,
                                                  RE::TESBoundObject* a_obj, std::int32_t a_targetCount) {
    if (!a_activatorRef->IsPlayerRef()) {
        return _Activate(a_this, a_targetRef, a_activatorRef, a_arg3, a_obj, a_targetCount);
    }
    if (auto base = Manager::GetSingleton()->GetFakeBound(a_targetRef)) {
        return _Activate(base, a_targetRef, a_activatorRef, a_arg3, a_obj, a_targetCount);
    }
    return _Activate(a_this, a_targetRef, a_activatorRef, a_arg3, a_obj, a_targetCount);
}

template <typename MenuType>
void Hooks::MenuHook<MenuType>::InstallHook(const REL::VariantID& varID) {
    REL::Relocation<std::uintptr_t> vTable(varID);
    _ProcessMessage = vTable.write_vfunc(0x4, &MenuHook<MenuType>::ProcessMessage_Hook);
}

int64_t Hooks::InventoryHoverHook::thunk(RE::InventoryEntryData* a1) {
    if (is_open.load() && inventory_loaded.load()) {
        if (const auto a_bound = a1->GetObject()) {
            const auto a_formid = a_bound->GetFormID();
            if (const auto mngr = Manager::GetSingleton();
                mngr->IsFakeContainer(a_formid)) {
                SkyPrompt::RegistrationPromptSink::GetSingleton()->Hide();
                SkyPrompt::MenuPromptSink::GetSingleton()->Show(a_bound);
            } else if (mngr->IsRealContainer(a_formid)) {
                SkyPrompt::MenuPromptSink::GetSingleton()->Hide();
                SkyPrompt::RegistrationPromptSink::GetSingleton()->Show(a_bound);
            } else {
                SkyPrompt::RegistrationPromptSink::GetSingleton()->Hide();
                SkyPrompt::MenuPromptSink::GetSingleton()->Hide();
            }
        }
    }
    return originalFunction(a1);
}

void Hooks::OnIsWorn(RE::TESBoundObject* object_to_equip) {
    if (object_to_equip->GetFormType() == RE::FormType::Armor) {
        RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
        RE::TESObjectARMO* armor = object_to_equip->As<RE::TESObjectARMO>();
        RE::TESRace* race = player->GetRace();
        RE::TESObjectARMA* armorAddon = armor->GetArmorAddon(race);
        char addonString[MAX_PATH]{'\0'};
        armorAddon->GetNodeName(addonString, player, armor, -1.0f);
        if (const auto a_node = player->GetNodeByName(addonString)) {
            objectNode.reset(a_node);
        } else {
            logger::warn("Failed to get node by name: {}", addonString);
        }
    }
}