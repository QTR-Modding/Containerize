#include "Hooks.h"
#include "MCP.h"
#include "SkyPrompt.h"
#include "Translations.h"

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        SpeedProfiler prof("Loading Sources");
        // Start
        if (SkyPrompt::g_clientID == 0) {
            SkyPrompt::g_clientID = SkyPromptAPI::RequestClientID();
        }
        if (SkyPrompt::g_clientID > 0) {
            Manager::GetSingleton()->Init();
            Settings::LoadTranslations();
            UI::Register();

            EventSink::GetSingleton()->Install();
            logger::info("EventSinks added.");
        } else {
            logger::error("Failed to get client ID from SkyPrompt API. Plugin will not work properly.");
        }
        ModCompatibility::Load();
        SKSE::Translation::ParseTranslation("Containerize");
    }
    if (message->type == SKSE::MessagingInterface::kPostLoadGame ||
        message->type == SKSE::MessagingInterface::kNewGame) {
    }
    if (message->type == SKSE::MessagingInterface::kPostPostLoad) {
        if (ModCompatibility::Mods::po3_use_or_take) {
            Hooks::InstallUseOrTakeHooks();
            logger::info("Use or Take hooks installed.");
        }
    }
}


SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SpeedProfiler prof("PluginLoad");
    SetupLog();
    SKSE::Init(skse);
    ModCompatibility::MakeChecks();
    if (!Settings::po3installed) {
        logger::critical("Latest version of Po3's Tweaks is not installed.");
        MsgBoxesNotifs::Windows::Po3ErrMsg();
        return false;
    }
    Hooks::Install();
    Settings::LoadOtherSettings();
    InitializeSerialization();
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}