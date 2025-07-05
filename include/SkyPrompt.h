#pragma once
#include "ClibUtil/singleton.hpp"
#include "SkyPrompt/API.hpp"

namespace SkyPrompt {
    class MyPromptSink final : public SkyPromptAPI::PromptSink,
                               public clib_util::singleton::ISingleton<MyPromptSink>
    {
	    SkyPromptAPI::Prompt open_prompt{ "Open",0,0, SkyPromptAPI::PromptType::kHold};
	    SkyPromptAPI::Prompt rename_prompt{ "Rename",1,0, SkyPromptAPI::PromptType::kHold};

		std::array<SkyPromptAPI::Prompt, 2> prompts = { open_prompt, rename_prompt };

    public:
        void ProcessEvent(SkyPromptAPI::PromptEvent event) const override;
	    std::span<const SkyPromptAPI::Prompt> GetPrompts() const override { return prompts; }
    };

	class MyPromptSink2 final : public SkyPromptAPI::PromptSink,
                               public clib_util::singleton::ISingleton<MyPromptSink2>
    {
		std::array<std::pair<RE::INPUT_DEVICE, uint32_t>, 2> keys = { {
		    std::make_pair(RE::INPUT_DEVICE::kKeyboard,SkyPromptAPI::kSkyrim),
	        std::make_pair(RE::INPUT_DEVICE::kGamepad,SkyPromptAPI::kSkyrim)
	    } };

		std::string weight_text;
		std::string value_text;

	    SkyPromptAPI::Prompt weight_prompt{ weight_text,2,0, SkyPromptAPI::PromptType::kSinglePress,0, keys};
	    SkyPromptAPI::Prompt value_prompt{ value_text ,3, 0, SkyPromptAPI::PromptType::kSinglePress, 0, keys};

		std::array<SkyPromptAPI::Prompt, 2> prompts = { weight_prompt, value_prompt };


    public:
        void ProcessEvent(SkyPromptAPI::PromptEvent) const override {}
	    std::span<const SkyPromptAPI::Prompt> GetPrompts() const override { return prompts; }
		void Start(const RE::TESObjectREFR* a_ref);
    };

	class MenuPromptSink final : public SkyPromptAPI::PromptSink,
                               public clib_util::singleton::ISingleton<MenuPromptSink>
    {
	    mutable SkyPromptAPI::Prompt open_prompt{ "Open",0,0, SkyPromptAPI::PromptType::kHold};
		mutable std::array<SkyPromptAPI::Prompt, 1> prompts = { open_prompt };

    public:
        void ProcessEvent(SkyPromptAPI::PromptEvent event) const override;
	    std::span<const SkyPromptAPI::Prompt> GetPrompts() const override { return prompts; }
		void Show() const;
		void Hide() const;
    };

    inline SkyPromptAPI::ClientID g_clientID = 0;

    inline std::array blockedMenus = {
									RE::DialogueMenu::MENU_NAME,
									RE::JournalMenu::MENU_NAME,
									RE::MapMenu::MENU_NAME,
									RE::StatsMenu::MENU_NAME,
									RE::ContainerMenu::MENU_NAME,
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
}
