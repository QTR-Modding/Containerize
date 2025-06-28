#pragma once
#include <ClibUtil/singleton.hpp>
#include <windows.h>
#include "Manager.h"

namespace SkyPromptAPI {

    #define DECLARE_API_FUNC_EX(                               \
        localName,   /* The name you call in your code */      \
        hostName,    /* The name actually exported by DLL */   \
        returnType,                                            \
        defaultValue,                                           \
        signature,   /* Parameter list in parentheses */        \
        callArgs     /* Just the parameter names in parentheses */ \
    )                                                          \
    using _##localName = returnType (*) signature;             \
    [[nodiscard]] inline returnType localName signature {      \
        static auto dllHandle = GetModuleHandle(L"SkyPrompt"); \
        if (!dllHandle) {                                   \
            return defaultValue;                               \
        }                                                      \
        static auto func =                                     \
            reinterpret_cast<_##localName>(GetProcAddress(dllHandle, hostName)); \
        if (func) {                                            \
            return func callArgs;                              \
        }                                                      \
        return defaultValue;                                   \
    }

	using ClientID = uint16_t;
	using EventID = uint16_t;
	using ActionID = uint16_t;
	using ButtonID = uint32_t; // RE::BSWin32KeyboardDevice::Key, RE::BSWin32MouseDevice::Key, RE::BSWin32GamepadDevice::Key, RE::BSPCOrbisGamepadDevice::Key

    constexpr ButtonID kMouseMove = 283;
    constexpr ButtonID kThumbstickMove = 284;

    enum PromptType {
        kSinglePress,
        kHold,
        kHoldAndKeep,
    };

	struct Prompt {
		std::string_view text;
        std::span<std::pair<RE::INPUT_DEVICE, ButtonID>> button_key;
        EventID eventID;
		ActionID actionID;
		PromptType type;
		RE::FormID refid;
	};

	enum PromptEventType {
		kAccepted,
		kDeclined,
        kRemovedByMod,
		kTimingOut,
		kTimeout,
		kDown,
        kUp,
		kMove
	};

    struct PromptEvent {
		Prompt prompt;
		PromptEventType type;
        std::pair<float,float> delta;
	};

    class PromptSink {
    public:
		virtual void ProcessEvent(PromptEvent event) = 0;
		virtual std::span<const Prompt> GetPrompts() = 0;
    protected:
        virtual ~PromptSink() = default;
    };

    DECLARE_API_FUNC_EX(
        RequestClientID,                          /* localName */
        "ProcessRequestClientID",                     /* hostName */
        ClientID,                                       /* returnType */
        0,                                      /* defaultValue */
        (), /* signature */
        ()         /* callArgs */
    );

    DECLARE_API_FUNC_EX(
        SendPrompt,                          /* localName */
        "ProcessSendPrompt",                     /* hostName */
        bool,                                       /* returnType */
        false,                                      /* defaultValue */
        (PromptSink* a_sink, ClientID a_clientID), /* signature */
        (a_sink, a_clientID)         /* callArgs */
    );

    DECLARE_API_FUNC_EX(
        RemovePrompt,                          /* localName */
        "ProcessRemovePrompt",                     /* hostName */
        void,                                       /* returnType */
        ,                                      /* defaultValue */
        (PromptSink* a_sink, ClientID a_clientID), /* signature */
        (a_sink, a_clientID)         /* callArgs */
    );

};



class MyPromptSink final : public SkyPromptAPI::PromptSink,
                           public clib_util::singleton::ISingleton<MyPromptSink>
{
	Manager* M = nullptr;

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
		{RE::INPUT_DEVICE::kGamepad, SkyPromptAPI::kThumbstickMove},
		} };


    std::array<SkyPromptAPI::Prompt, 4> prompts = {{
        {.text = "Open", .button_key = keys1,.eventID=0, .actionID=0, .type= SkyPromptAPI::PromptType::kSinglePress, .refid=0},
        {.text = "Take", .button_key = {},.eventID=1, .actionID=0, .type= SkyPromptAPI::PromptType::kSinglePress, .refid=0},
        {.text = "Rename", .button_key = {},.eventID=2, .actionID=0, .type= SkyPromptAPI::PromptType::kHold, .refid=0},
        {.text = "asd", .button_key = {},.eventID=2, .actionID=1, .type= SkyPromptAPI::PromptType::kHoldAndKeep, .refid=0},
    }};

public:
	void SetManager(Manager* mngr) { M = mngr; }
    void ProcessEvent(SkyPromptAPI::PromptEvent event) override;
	std::span<const SkyPromptAPI::Prompt> GetPrompts() override { return prompts; }
	void SetRef(const RefID a_refid);
    void UnSetRef();
};

inline SkyPromptAPI::ClientID g_clientID = 0;