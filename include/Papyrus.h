#pragma once


namespace Papyrus {
    using VM = RE::BSScript::Internal::VirtualMachine;
    using ObjectPtr = RE::BSTSmartPointer<RE::BSScript::Object>;
    using CallBack = RE::BSScript::IStackCallbackFunctor;

    inline RE::VMHandle GetHandle(const RE::TESForm* a_form);

    inline ObjectPtr GetObjectPtr(const RE::TESForm* a_form, const char* a_class, bool a_create);

    template <class... Args>
    bool CallFunction(const std::string_view functionClass, const std::string_view function,
                      const std::optional<CallBack*>& a_callback, Args... a_args) {
        const auto skyrimVM = RE::SkyrimVM::GetSingleton();
        if (const auto vm = skyrimVM ? skyrimVM->impl : nullptr) {
            RE::BSTSmartPointer<CallBack> callback;
            if (a_callback.has_value()) {
                callback.reset(a_callback.value());
            }
            auto args = RE::MakeFunctionArguments(std::forward<Args>(a_args)...);
            return vm->DispatchStaticCall(std::string(functionClass).c_str(), std::string(function).c_str(), args,
                                          callback);
        }
        return false;
    }

    // Thanks and credits to Bloc: https://discord.com/channels/874895328938172446/945560222670393406/1093262407989731338
    class ConversationCallbackFunctor final : public RE::BSScript::IStackCallbackFunctor {
        std::string rename;
        RE::TESBoundObject* fake;

        void operator()(RE::BSScript::Variable a_result) override;

        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {
        }

    public:
        explicit ConversationCallbackFunctor(RE::TESBoundObject* a_fake) : fake(a_fake) {
        }
    };

    class RenameCallbackFunctor final : public RE::BSScript::IStackCallbackFunctor {
        RE::TESBoundObject* fake;

        void operator()([[maybe_unused]] const RE::BSScript::Variable a_result) override {
            OnRename();
        }

        void OnRename();

        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {
        }

    public:
        explicit RenameCallbackFunctor(RE::TESBoundObject* a_fake) : fake(a_fake) {
        }
    };
}