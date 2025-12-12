#pragma once
#include "REX/REX/Singleton.h"

class EventSink final : public REX::Singleton<EventSink>,
                        public RE::BSTEventSink<RE::TESFurnitureEvent>,
                        public RE::BSTEventSink<SKSE::CrosshairRefEvent>,
                        public RE::BSTEventSink<RE::TESFormDeleteEvent>,
                        public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    std::atomic<bool> block_droptake = false;

    RE::NiPointer<RE::TESObjectREFR> furniture;

    static void SendPrompts(RE::TESObjectREFR* a_container);

public:
    std::atomic<bool> furniture_entered = false;

    void Reset();

    RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* a_event,
                                          RE::BSTEventSource<SKSE::CrosshairRefEvent>* a_eventSource) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESFurnitureEvent* event,
                                          RE::BSTEventSource<RE::TESFurnitureEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* a_event,
                                          RE::BSTEventSource<RE::TESFormDeleteEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource) override;

    static void RemovePrompts();
    static void RemoveMenuPrompts();

    void Install();
};