#include "Hooks.h"

void Hooks::Install()
{
	MoveItemHooks<RE::PlayerCharacter>::install();
	MoveItemHooks<RE::TESObjectREFR>::install(false);
	MoveItemHooks<RE::Character>::install();

	auto& trampoline = SKSE::GetTrampoline();
    constexpr size_t size_per_hook = 14;
	trampoline.create(size_per_hook*1);

	const REL::Relocation<std::uintptr_t> target3{REL::RelocationID(67315, 68617)};
    InputHook::func = trampoline.write_call<5>(target3.address() + 0x7B, InputHook::thunk);

	MenuHook<RE::ContainerMenu>::InstallHook(RE::VTABLE_ContainerMenu[0]);
}

template<typename RefType>
void Hooks::MoveItemHooks<RefType>::pickUpObject(RefType * a_this, RE::TESObjectREFR * a_object, int32_t a_count, bool a_arg3, bool a_play_sound)
{
	if (!a_object || a_count>1) {
		return pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
	}
	logger::info("Pickup event.");
	M->HandlePickup(a_this, a_object);

	pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
}

template<typename RefType>
void Hooks::MoveItemHooks<RefType>::addObjectToContainer(RefType* a_this, RE::TESBoundObject* a_object, RE::ExtraDataList* a_extraList, std::int32_t a_count, RE::TESObjectREFR* a_fromRefr)
{
	return add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);
}

template<typename RefType>
RE::ObjectRefHandle * Hooks::MoveItemHooks<RefType>::RemoveItem(RefType * a_this, RE::ObjectRefHandle & a_hidden_return_argument, RE::TESBoundObject * a_item, std::int32_t a_count, RE::ITEM_REMOVE_REASON a_reason, RE::ExtraDataList * a_extra_list, RE::TESObjectREFR * a_move_to_ref, const RE::NiPoint3 * a_drop_loc, const RE::NiPoint3 * a_rotate)
{
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

	auto ui = RE::UI::GetSingleton();
	if (ui->IsItemMenuOpen() || ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) {
		return func(a_dispatcher, a_event);
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
	return block;
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
	if (const std::string_view menuname = MenuType::MENU_NAME; a_message.menu==menuname) {
	    if (menuname == RE::ContainerMenu::MENU_NAME) {
			if (const auto msg_type = static_cast<int>(a_message.type.get()); msg_type == 3) { // closing
				M->HandleContainerMenuExit();
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
