#pragma once
#include "Hooks.h"
#include "Manager.h"

// Thanks and credits to Bloc: https://discord.com/channels/874895328938172446/945560222670393406/1093262407989731338
class ConversationCallbackFunctor final : public RE::BSScript::IStackCallbackFunctor {

    std::string rename;
	Manager* M;

    void operator()(const RE::BSScript::Variable a_result) override {
        if (a_result.IsNoneObject()) {
            logger::trace("Result: None");
        } else if (a_result.IsString()) {
            rename = a_result.GetString();
            logger::trace("Result: {}", rename);
            if (!rename.empty()) {
				M->RenameContainer(rename);
			}
        }
    }

    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

public:
    explicit ConversationCallbackFunctor(Manager* mngr) : M(mngr) {}
};

class OurEventSink final : public RE::BSTEventSink<RE::TESFurnitureEvent>,
                           public RE::BSTEventSink<RE::TESFormDeleteEvent> {

    OurEventSink() = default;
    OurEventSink(const OurEventSink&) = delete;
    OurEventSink(OurEventSink&&) = delete;
    OurEventSink& operator=(const OurEventSink&) = delete;
    OurEventSink& operator=(OurEventSink&&) = delete;


	std::atomic<bool> block_droptake = false;
	std::atomic<bool> listen_menu_close = true;



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