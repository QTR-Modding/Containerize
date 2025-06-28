#pragma once
#include "Manager.h"

class OurEventSink final : public RE::BSTEventSink<RE::TESFurnitureEvent>,
                            public RE::BSTEventSink<SKSE::CrosshairRefEvent>,
                           public RE::BSTEventSink<RE::TESFormDeleteEvent> {

    OurEventSink() = default;
    OurEventSink(const OurEventSink&) = delete;
    OurEventSink(OurEventSink&&) = delete;
    OurEventSink& operator=(const OurEventSink&) = delete;
    OurEventSink& operator=(OurEventSink&&) = delete;


	std::atomic<bool> block_droptake = false;

    RE::NiPointer<RE::TESObjectREFR> furniture;

public:

	std::atomic<bool> furniture_entered = false;
    Manager* M = nullptr;

    explicit OurEventSink(Manager* mngr)
        :  M(mngr){}

    static OurEventSink* GetSingleton(Manager* manager) {
        static OurEventSink singleton(manager);
        return &singleton;
    }

    void Reset();

    RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* a_event, RE::BSTEventSource<SKSE::CrosshairRefEvent>* a_eventSource) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESFurnitureEvent* event,
                                          RE::BSTEventSource<RE::TESFurnitureEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* a_event,
                                          RE::BSTEventSource<RE::TESFormDeleteEvent>*) override;
};