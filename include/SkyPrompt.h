#pragma once
#include "REX/REX/Singleton.h"
#include "SkyPrompt/API.hpp"

namespace SkyPrompt {
    namespace Strings {
        inline std::string open_bag = "$quantCTRZOpen";
        inline std::string rename_bag = "$quantCTRZRename";
        inline std::string weight = "$quantCTRZWeight";
        inline std::string value = "$quantCTRZValue";
    };

    inline std::array<std::pair<RE::INPUT_DEVICE, uint32_t>, 2> akatosh_keys = {{
        std::make_pair(RE::INPUT_DEVICE::kKeyboard, SkyPromptAPI::kSkyrim),
        std::make_pair(RE::INPUT_DEVICE::kGamepad, SkyPromptAPI::kSkyrim)
    }};

    class MyPromptSink final : public SkyPromptAPI::PromptSink,
                               public REX::Singleton<MyPromptSink> {
        
        std::string weight_text;
        std::string value_text;
        RefID refid=0;

        SkyPromptAPI::Prompt open_prompt{Strings::open_bag, 0, 0, SkyPromptAPI::PromptType::kHold, refid};
        SkyPromptAPI::Prompt rename_prompt{Strings::rename_bag, 1, 0, SkyPromptAPI::PromptType::kHold, refid};
        SkyPromptAPI::Prompt weight_prompt{weight_text, 2, 0, SkyPromptAPI::PromptType::kSinglePress, refid, akatosh_keys};
        SkyPromptAPI::Prompt value_prompt{value_text, 3,           0, SkyPromptAPI::PromptType::kSinglePress, refid, akatosh_keys};

        std::array<SkyPromptAPI::Prompt, 4> prompts = {open_prompt, rename_prompt, weight_prompt, value_prompt};

    public:
        void Start(RE::TESObjectREFR* a_ref);
        void ProcessEvent(SkyPromptAPI::PromptEvent event) const override;
        std::span<const SkyPromptAPI::Prompt> GetPrompts() const override { return prompts; }
    };

    class MenuPromptSink final : public SkyPromptAPI::PromptSink,
                                 public REX::Singleton<MenuPromptSink> {
        mutable std::string weight_text;

        mutable SkyPromptAPI::Prompt open_prompt{Strings::open_bag, 0, 0, SkyPromptAPI::PromptType::kHold};
        mutable SkyPromptAPI::Prompt rename_prompt{Strings::rename_bag, 1, 0, SkyPromptAPI::PromptType::kHold};
        mutable SkyPromptAPI::Prompt weight_prompt{weight_text, 2, 0, SkyPromptAPI::PromptType::kSinglePress, 0,
                                                   akatosh_keys};
        mutable std::array<SkyPromptAPI::Prompt, 3> prompts = {open_prompt, rename_prompt, weight_prompt};

    public:
        void ProcessEvent(SkyPromptAPI::PromptEvent event) const override;
        std::span<const SkyPromptAPI::Prompt> GetPrompts() const override { return prompts; }
        void Show(const RE::TESBoundObject* a_fake) const;
        void Hide() const;

        static void OpenBag(RE::TESBoundObject* a_fake, bool is_worn);
    };

    class RegistrationPromptSink final : public SkyPromptAPI::PromptSink,
                                         public REX::Singleton<RegistrationPromptSink> {
        mutable std::string weight_text;

        mutable SkyPromptAPI::Prompt open_prompt{Strings::open_bag, 3, 0, SkyPromptAPI::PromptType::kHold};
        mutable SkyPromptAPI::Prompt rename_prompt{Strings::rename_bag, 4, 0, SkyPromptAPI::PromptType::kHold};
        mutable SkyPromptAPI::Prompt weight_prompt{weight_text, 5, 0, SkyPromptAPI::PromptType::kSinglePress, 0,
                                                   akatosh_keys};
        mutable std::array<SkyPromptAPI::Prompt, 3> prompts = {open_prompt, rename_prompt, weight_prompt};

        mutable RE::TESObjectREFRPtr containermenu_owner = nullptr;

    public:
        void ProcessEvent(SkyPromptAPI::PromptEvent event) const override;
        std::span<const SkyPromptAPI::Prompt> GetPrompts() const override { return prompts; }
        void Show(const RE::TESBoundObject* a_item) const;
        void Hide() const;
    };

    inline SkyPromptAPI::ClientID g_clientID = 0;

    inline std::array blockedMenus = {
        RE::DialogueMenu::MENU_NAME,
        RE::JournalMenu::MENU_NAME,
        RE::MapMenu::MENU_NAME,
        RE::StatsMenu::MENU_NAME,
        RE::ContainerMenu::MENU_NAME,
        RE::BarterMenu::MENU_NAME,
        RE::CraftingMenu::MENU_NAME,
        RE::InventoryMenu::MENU_NAME,
        RE::TweenMenu::MENU_NAME,
        RE::TrainingMenu::MENU_NAME,
        RE::TutorialMenu::MENU_NAME,
        RE::LockpickingMenu::MENU_NAME,
        RE::SleepWaitMenu::MENU_NAME,
        RE::LevelUpMenu::MENU_NAME,
        RE::Console::MENU_NAME,
        RE::BookMenu::MENU_NAME,
        RE::CreditsMenu::MENU_NAME,
        RE::LoadingMenu::MENU_NAME,
        RE::MessageBoxMenu::MENU_NAME,
        RE::MainMenu::MENU_NAME,
        RE::RaceSexMenu::MENU_NAME,
    };

    bool IsAnyMenuOpen();

    inline std::map<FormID, RE::NiPointer<RE::NiAVObject>> saved_models;
}