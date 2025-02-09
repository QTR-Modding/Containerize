#pragma once
#include "Manager.h"

class OurEventSink final : public RE::BSTEventSink<RE::TESFurnitureEvent>,
                           public RE::BSTEventSink<RE::TESFormDeleteEvent> {

    OurEventSink() = default;
    OurEventSink(const OurEventSink&) = delete;
    OurEventSink(OurEventSink&&) = delete;
    OurEventSink& operator=(const OurEventSink&) = delete;
    OurEventSink& operator=(OurEventSink&&) = delete;


	std::atomic<bool> block_droptake = false;

    std::string ReShowMenu;

    RE::NiPointer<RE::TESObjectREFR> furniture;

    RE::BSEventNotifyControl OnRename() const;

public:

	std::atomic<bool> block_eventsinks = false;
	std::atomic<bool> furniture_entered = false;
	std::atomic<bool> listen_weight_limit = false;
    Manager* M = nullptr;

    explicit OurEventSink(Manager* mngr)
        :  M(mngr){}

    static OurEventSink* GetSingleton(Manager* manager) {
        static OurEventSink singleton(manager);
        return &singleton;
    }

	void SetBlockSinks(const bool block) {
		block_eventsinks = block;
	}

    void Reset();

    RE::BSEventNotifyControl ProcessEvent(const RE::TESFurnitureEvent* event,
                                          RE::BSTEventSource<RE::TESFurnitureEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* a_event,
                                          RE::BSTEventSource<RE::TESFormDeleteEvent>*) override;
};