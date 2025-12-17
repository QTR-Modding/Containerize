#include "Events.h"
#include "SkyPrompt.h"

void EventSink::SendPrompts(RE::TESObjectREFR* a_container) {
    if (SkyPrompt::IsAnyMenuOpen()) {
        return;
    }
    if (!SkyPromptAPI::SendPrompt(SkyPrompt::MyPromptSink::GetSingleton(), SkyPrompt::g_clientID)) {
		//logger::error("Prompt failed.");
	}
    const auto ps = SkyPrompt::MyPromptSink2::GetSingleton();
    ps->Start(a_container);
    if (!SkyPromptAPI::SendPrompt(ps, SkyPrompt::g_clientID)) {
		//logger::error("Prompt failed.");
	}
}

void EventSink::RemovePrompts()
{
    SkyPromptAPI::RemovePrompt(SkyPrompt::MyPromptSink::GetSingleton(), SkyPrompt::g_clientID);
    SkyPromptAPI::RemovePrompt(SkyPrompt::MyPromptSink2::GetSingleton(), SkyPrompt::g_clientID);
}

void EventSink::RemoveMenuPrompts()
{
    SkyPrompt::MenuPromptSink::GetSingleton()->Hide();
	SkyPrompt::RegistrationPromptSink::GetSingleton()->Hide();
}

void EventSink::Reset() {
	furniture = nullptr;
	furniture_entered.store(false);
	block_droptake.store(false);
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const SKSE::CrosshairRefEvent* a_event, RE::BSTEventSource<SKSE::CrosshairRefEvent>*)
{
	if (!a_event->crosshairRef) {
	    RemovePrompts();
        return RE::BSEventNotifyControl::kContinue;
	}
    if (const auto ref = a_event->crosshairRef.get()) {
		Manager::GetSingleton()->HandleFakePlacement(ref);
    }
	if (const auto baseform = DynamicFormTracker::GetSingleton()->GetOGFormOfDynamic(a_event->crosshairRef->GetBaseObject()->GetFormID())) {
        logger::warn("Fake object not found in ChestToFakeContainer.");
	    WorldObject::SwapObjects(a_event->crosshairRef.get(), skyrim_cast<RE::TESBoundObject*>(baseform), false);    
	}

    const auto M = Manager::GetSingleton();

    if (M->IsChestMenuQueued() || M->IsInChestMenu()) {
        RemovePrompts();
    }
    else if (M->IsRealContainer(a_event->crosshairRef.get())) {
		SendPrompts(a_event->crosshairRef.get());
    }
    else {
        RemovePrompts();
    }


	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const RE::TESFurnitureEvent* event,
    RE::BSTEventSource<RE::TESFurnitureEvent>*) {
        
    if (!event || !event->actor->IsPlayerRef()) return RE::BSEventNotifyControl::kContinue;
    if (furniture_entered && event->type == RE::TESFurnitureEvent::FurnitureEventType::kEnter)
        return RE::BSEventNotifyControl::kContinue;
    if (!furniture_entered && event->type == RE::TESFurnitureEvent::FurnitureEventType::kExit)
        return RE::BSEventNotifyControl::kContinue;

    const auto furn_base = event->targetFurniture->GetBaseObject();
    if (!furn_base->Is(RE::TESFurniture::FORMTYPE)) return RE::BSEventNotifyControl::kContinue;

    if (event->type == RE::TESFurnitureEvent::FurnitureEventType::kEnter) {
        furniture_entered = true;
        furniture = event->targetFurniture;
        Manager::GetSingleton()->HandleCraftingEnter(furniture->GetFormID());
    }
    else if (event->type == RE::TESFurnitureEvent::FurnitureEventType::kExit) {
        if (event->targetFurniture == furniture) {
            Manager::GetSingleton()->HandleCraftingExit();
            furniture_entered = false;
            furniture = nullptr;
        }
    }
    else {
        logger::trace("Furniture event: Unknown");
    }


    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const RE::TESFormDeleteEvent* a_event,
    RE::BSTEventSource<RE::TESFormDeleteEvent>*) {
    if (!a_event) return RE::BSEventNotifyControl::kContinue;
    if (!a_event->formID) return RE::BSEventNotifyControl::kContinue;
    Manager::GetSingleton()->HandleFormDelete(a_event->formID);
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
{
	if (!a_event) return RE::BSEventNotifyControl::kContinue;
    RemoveMenuPrompts();
	RemovePrompts();

	return RE::BSEventNotifyControl::kContinue;
}

void EventSink::Install() {
    const auto eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
    eventSourceHolder->AddEventSink<RE::TESFurnitureEvent>(this);
    eventSourceHolder->AddEventSink<RE::TESFormDeleteEvent>(this);
	SKSE::GetCrosshairRefEventSource()->AddEventSink(this);
	RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(this);
}
