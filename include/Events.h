#pragma once
#include "Manager.h"

class EventSink final : public clib_util::singleton::ISingleton<EventSink>,
                            public RE::BSTEventSink<RE::TESFurnitureEvent>,
                            public RE::BSTEventSink<SKSE::CrosshairRefEvent>,
                           public RE::BSTEventSink<RE::TESFormDeleteEvent> {

	std::atomic<bool> block_droptake = false;

    RE::NiPointer<RE::TESObjectREFR> furniture;

public:

	std::atomic<bool> furniture_entered = false;

    void Reset();

    RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* a_event, RE::BSTEventSource<SKSE::CrosshairRefEvent>* a_eventSource) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESFurnitureEvent* event,
                                          RE::BSTEventSource<RE::TESFurnitureEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* a_event,
                                          RE::BSTEventSource<RE::TESFormDeleteEvent>*) override;
};