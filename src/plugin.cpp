#include "Hooks.h"
#include "MCP.h"
#include "Translations.h"

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        SpeedProfiler prof("Loading Sources");
        // Start

        Manager::GetSingleton()->Init();
        auto eventSink = EventSink::GetSingleton();
	    LoadTranslations();
        UI::Register();

        auto* eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
        eventSourceHolder->AddEventSink<RE::TESFurnitureEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESFormDeleteEvent>(eventSink);
		SKSE::GetCrosshairRefEventSource()->AddEventSink(eventSink);
		logger::info("EventSinks added.");
    }
    if (message->type == SKSE::MessagingInterface::kPostLoadGame ||
        message->type == SKSE::MessagingInterface::kNewGame) {
    }
}


SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SpeedProfiler prof("PluginLoad");
    SetupLog();
    SKSE::Init(skse);
    if (!IsPo3Installed()) {
		logger::critical("Latest version of Po3's Tweaks is not installed.");
		MsgBoxesNotifs::Windows::Po3ErrMsg();
		Settings::po3installed = false;
		return false;
    }
	Settings::po3installed = true;
    LoadOtherSettings();
    Serialization::InitializeSerialization();
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	Hooks::Install();
    return true;
}