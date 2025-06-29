#pragma once

#include "ClibUtil/singleton.hpp"
#include "SkyPrompt/API.hpp"

namespace SkyPrompt {
    class MyPromptSink final : public SkyPromptAPI::PromptSink,
                               public clib_util::singleton::ISingleton<MyPromptSink>
    {
	    std::array<std::pair<RE::INPUT_DEVICE, uint32_t>, 2> keys1 = { {
		    {RE::INPUT_DEVICE::kKeyboard, RE::BSKeyboardDevice::Keys::kC},
		    {RE::INPUT_DEVICE::kGamepad, REX::W32::XINPUT_GAMEPAD_BACK},
	    } };
	    std::array<std::pair<RE::INPUT_DEVICE, uint32_t>, 2> keys2 = { {
		    {RE::INPUT_DEVICE::kKeyboard, RE::BSKeyboardDevice::Keys::kT},
		    {RE::INPUT_DEVICE::kGamepad, REX::W32::XINPUT_GAMEPAD_DPAD_DOWN},
		    } };
	    std::array<std::pair<RE::INPUT_DEVICE, uint32_t>, 2> keys3 = { {
		    {RE::INPUT_DEVICE::kMouse, SkyPromptAPI::kMouseMove},
		    {RE::INPUT_DEVICE::kGamepad, SkyPromptAPI::kThumbstickMoveR},
		    } };

	    SkyPromptAPI::Prompt open_prompt{ "Open",0,0, SkyPromptAPI::PromptType::kHold};
	    SkyPromptAPI::Prompt rename_prompt{ "Rename",1,0, SkyPromptAPI::PromptType::kHold};

        std::array<SkyPromptAPI::Prompt, 2> prompts = {open_prompt,rename_prompt};

    public:
        void ProcessEvent(SkyPromptAPI::PromptEvent event) const override;
	    std::span<const SkyPromptAPI::Prompt> GetPrompts() const override { return prompts; }
	    void SetRef(RefID a_refid);
        void UnSetRef();
    };

    inline SkyPromptAPI::ClientID g_clientID = 0;
}
