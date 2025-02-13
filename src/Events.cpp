#include "Events.h"

void OurEventSink::Reset() {
	furniture = nullptr;
	furniture_entered.store(false);
	block_droptake.store(false);
}

RE::BSEventNotifyControl OurEventSink::ProcessEvent(const SKSE::CrosshairRefEvent* a_event, RE::BSTEventSource<SKSE::CrosshairRefEvent>*)
{
	if (!a_event->crosshairRef) return RE::BSEventNotifyControl::kContinue;
    if (const auto ref = a_event->crosshairRef.get()) {
		M->HandleFakePlacement(ref);
    }
	if (const auto baseform = DynamicFormTracker::GetSingleton()->GetOGFormOfDynamic(a_event->crosshairRef.get()->GetBaseObject()->GetFormID())) {
        logger::warn("Fake object not found in ChestToFakeContainer.");
	    WorldObject::SwapObjects(a_event->crosshairRef.get(), skyrim_cast<RE::TESBoundObject*>(baseform), false);    
	}
	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl OurEventSink::ProcessEvent(const RE::TESFurnitureEvent* event,
    RE::BSTEventSource<RE::TESFurnitureEvent>*) {
        
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

RE::BSEventNotifyControl OurEventSink::ProcessEvent(const RE::TESFormDeleteEvent* a_event,
    RE::BSTEventSource<RE::TESFormDeleteEvent>*) {
    if (!a_event) return RE::BSEventNotifyControl::kContinue;
    if (!a_event->formID) return RE::BSEventNotifyControl::kContinue;
    M->HandleFormDelete(a_event->formID);
    return RE::BSEventNotifyControl::kContinue;
}