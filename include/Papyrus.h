#pragma once

// Thanks and credits to Bloc: https://discord.com/channels/874895328938172446/945560222670393406/1093262407989731338
class ConversationCallbackFunctor final : public RE::BSScript::IStackCallbackFunctor {

    std::string rename;

    void operator()(const RE::BSScript::Variable a_result) override;

    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

public:
    ConversationCallbackFunctor(){}
};

class RenameCallbackFunctor final : public RE::BSScript::IStackCallbackFunctor {

	void operator()([[maybe_unused]] const RE::BSScript::Variable a_result) override {
        OnRename();
    }

    static void OnRename();

    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}


public:
	RenameCallbackFunctor(){};
};