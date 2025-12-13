#pragma once
#include <REX/REX/Singleton.h>
#include "QuickLootAPI.h"
#include "SkyPrompt.h"
#include "SkyPrompt/API.hpp"

class QuickLootHelper :
    public REX::Singleton<QuickLootHelper>,
    public RE::BSTEventSink<SKSE::CrosshairRefEvent>,
    public SkyPromptAPI::PromptSink {

    mutable bool ql_menu_disabled = false;

public:
    enum QL_MenuState : std::uint8_t {
        kClosed=0,
        kOpen,
    };

    QL_MenuState GetLastOpenState() const { return last_open_state; }
    void SetLastOpenState(const QL_MenuState state) const { last_open_state = state; }

    static void containerOverrideHandler(QuickLoot::API::ContainerOverrideEvent* e);

    static void openingLootMenuHandler(QuickLoot::API::OpeningLootMenuEvent* e);

    static void closingLootMenuHandler(QuickLoot::API::CloseLootMenuEvent* e);

    void ProcessEvent(SkyPromptAPI::PromptEvent event) const override;
    std::span<const SkyPromptAPI::Prompt> GetPrompts() const override { return prompts; }

    RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* a_event,
                                          RE::BSTEventSource<SKSE::CrosshairRefEvent>* a_eventSource) override;

    //bool SendPrompt(RefID refid) const;
    void RemovePrompt() const;

private:
    mutable QL_MenuState last_open_state = kOpen;

    std::array<std::pair<RE::INPUT_DEVICE, uint32_t>, 2> prompt_keys = {
        {std::make_pair(RE::INPUT_DEVICE::kKeyboard, RE::BSWin32KeyboardDevice::Key::kF),
         std::make_pair(RE::INPUT_DEVICE::kGamepad, RE::BSWin32GamepadDevice::Key::kX)
        }
    };

    mutable SkyPromptAPI::Prompt ql_prompt{SkyPrompt::Strings::toggle_menu, 0, 0,
                                           SkyPromptAPI::PromptType::kSinglePress, 0, prompt_keys};
    
    mutable std::array<SkyPromptAPI::Prompt, 1> prompts = {ql_prompt};
    bool SendPrompt(RE::TESObjectREFR* a_ref) const;

    void ToggleQLMenu(bool a_disabled) const;
};