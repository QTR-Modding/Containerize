#include "Hooks.h"
#include "MCP.h"
#include "SkyPrompt.h"
#include "Translations.h"

namespace {
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void OnMessage(SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            SpeedProfiler prof("Loading Sources");
            // Start
            if (SkyPrompt::g_clientID == 0) {
                SkyPrompt::g_clientID = SkyPromptAPI::RequestClientID();
            }
            if (SkyPrompt::g_clientID > 0) {
                if (!Manager::GetSingleton()->Init()) {
                    logger::critical("Manager failed to initialize. Plugin will not work properly.");
                    MsgBoxesNotifs::InGame::InitErr();
                    return;
                }
                Settings::LoadTranslations();
                UI::Register();

                EventSink::GetSingleton()->Install();
                logger::info("EventSinks added.");
                
                ModCompatibility::Load();
                SKSE::Translation::ParseTranslation("Containerize");

            } else {
                logger::critical("Failed to get client ID from SkyPrompt API. Plugin will not work properly.");
                MsgBoxesNotifs::InGame::InitErr();
                Manager::GetSingleton()->Uninstall();
            }
        }
        if (message->type == SKSE::MessagingInterface::kPostPostLoad) {
            if (ModCompatibility::Mods::po3_use_or_take) {
                Hooks::InstallUseOrTakeHooks();
                logger::info("Use or Take hooks installed.");
            }
        }
    }
}


SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SpeedProfiler prof("PluginLoad");
    SetupLog();
    SKSE::Init(skse);
    if (!ModCompatibility::AreRequirementsInstalled()) {
        logger::critical("Required mods are not installed.");
        MsgBoxesNotifs::Windows::ReqErrMsg();
        return false;
    }
    Hooks::Install();
    Settings::LoadOtherSettings();
    InitializeSerialization();
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}