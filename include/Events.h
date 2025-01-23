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

class OurEventSink final : public RE::BSTEventSink<RE::TESEquipEvent>,
                           public RE::BSTEventSink<RE::TESActivateEvent>,
                           public RE::BSTEventSink<SKSE::CrosshairRefEvent>,
                           public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
                           public RE::BSTEventSink<RE::TESFurnitureEvent>,
                           public RE::BSTEventSink<RE::TESContainerChangedEvent>,
                           public RE::BSTEventSink<RE::InputEvent*>,
                           public RE::BSTEventSink<RE::TESFormDeleteEvent> {

    OurEventSink() = default;
    OurEventSink(const OurEventSink&) = delete;
    OurEventSink(OurEventSink&&) = delete;
    OurEventSink& operator=(const OurEventSink&) = delete;
    OurEventSink& operator=(OurEventSink&&) = delete;


	std::atomic<bool> block_droptake = false;
	std::atomic<bool> listen_menu_close = true;


    FormID fake_equipped_id = 0;
	FormID fake_id_ = 0;

	bool equipped = false;

    std::string ReShowMenu;

    RE::NiPointer<RE::TESObjectREFR> furniture;

    RE::BSEventNotifyControl OnRename() const;
    RE::BSEventNotifyControl ToggleEquipOpenContainer();
    void ReShow();
    bool HideMenuOnEquipHeld();

public:

	std::atomic<bool> block_eventsinks = false;
	std::atomic<bool> listen_crosshair_ref = true;
	std::atomic<bool> furniture_entered = false;
	std::atomic<bool> listen_weight_limit = false;
    Manager* M = nullptr;
    RefID external_container_refid = 0;  // set in input event

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

	// for opening container from inventory
    RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event, RE::BSTEventSource<RE::TESEquipEvent>*) override;

    // Prompts Messagebox
    RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* event,
                                          RE::BSTEventSource<RE::TESActivateEvent>*) override;

    // to disable ref activation and external container-fake container placement
    RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* event,
                                          RE::BSTEventSource<SKSE::CrosshairRefEvent>*) override;

    // to close chest and save the contents and remove items
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESFurnitureEvent* event,
                                          RE::BSTEventSource<RE::TESFurnitureEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* event,
                                          RE::BSTEventSource<RE::TESContainerChangedEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* evns, RE::BSTEventSource<RE::InputEvent*>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* a_event,
                                          RE::BSTEventSource<RE::TESFormDeleteEvent>*) override;
};