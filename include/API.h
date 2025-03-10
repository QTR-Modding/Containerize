#pragma once
#include <ClibUtil/singleton.hpp>
#include <windows.h>

#include "Manager.h"


namespace StreamlinedAPI {

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
        static auto dllHandle = GetModuleHandle(L"StreamlinedInteractions"); \
        static auto func =                                     \
            reinterpret_cast<_##localName>(GetProcAddress(dllHandle, hostName)); \
        if (func) {                                            \
            return func callArgs;                              \
        }                                                      \
        return defaultValue;                                   \
    }

	struct Prompt {
		using ButtonKey = std::map<RE::INPUT_DEVICE, uint32_t>;
		std::string text;
		std::optional<ButtonKey> button_key;
	};

    struct PromptEvent {
		Prompt prompt;
		int type; // 0 = accepted, 1 = declined, 2 = timeout
	};

    class PromptSink {
    public:
		virtual void ProcessEvent(PromptEvent event) = 0;
		virtual std::vector<Prompt>& GetPrompts() = 0;
    protected:
        virtual ~PromptSink() = default;
    };

    // 1) The macro name:       SendPrompt
    // 2) The return type:      bool
    // 3) The default value:    false
    // 4) The parameter list:   (PromptSink* a_sink, bool a_force)
    // 5) The call arguments:   (a_sink, a_force)

    DECLARE_API_FUNC_EX(
        SendPrompt,                          /* localName */
        "ProcessSendPrompt",                     /* hostName */
        bool,                                       /* returnType */
        false,                                      /* defaultValue */
        (PromptSink* a_sink, bool a_force), /* signature */
        (a_sink, a_force)         /* callArgs */
    );

    // 1) The macro name:       RemovePrompt
    // 2) The return type:      void
    // 3) The default value:    
    // 4) The parameter list:   (PromptSink* a_sink)
    // 5) The call arguments:   (a_sink)

    DECLARE_API_FUNC_EX(
        RemovePrompt,                          /* localName */
        "ProcessRemovePrompt",                     /* hostName */
        void,                                       /* returnType */
        ,                                      /* defaultValue */
        (PromptSink* a_sink), /* signature */
        (a_sink)         /* callArgs */
    );
};


class MyPromptSink final : public StreamlinedAPI::PromptSink,
                           public clib_util::singleton::ISingleton<MyPromptSink>
{
	Manager* M = nullptr;

	std::vector<StreamlinedAPI::Prompt> prompts = {
		{"Open", std::nullopt},
			{"Take", std::nullopt},
		{.text= "Rename", .button_key= std::nullopt}
	};
public:

	void SetManager(Manager* mngr) {
		M = mngr;
	}

    void ProcessEvent(StreamlinedAPI::PromptEvent event) override;


	std::vector<StreamlinedAPI::Prompt>& GetPrompts() override {
		return prompts;
	}
};