#include "Papyrus.h"
#include "Manager.h"

void ConversationCallbackFunctor::operator()(const RE::BSScript::Variable a_result) {
    if (a_result.IsNoneObject()) {
        logger::trace("Result: None");
    } else if (a_result.IsString()) {
        rename = a_result.GetString();
        logger::trace("Result rename: {}", rename);
        if (!rename.empty()) {
            Manager::GetSingleton()->RenameContainer(rename);
            return;
        }
    }
    Manager::GetSingleton()->MsgBoxCallback(3);
}

void RenameCallbackFunctor::OnRename() {
    logger::trace("Rename menu closed.");
    const auto skyrimVM = RE::SkyrimVM::GetSingleton();
    if (const auto vm = skyrimVM ? skyrimVM->impl : nullptr) {
        const char* menuID = "UITextEntryMenu";
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new ConversationCallbackFunctor());
        const auto args = RE::MakeFunctionArguments(std::move(menuID));
        if (!vm->DispatchStaticCall("UIExtensions", "GetMenuResultString", args, callback)) {
            Manager::GetSingleton()->MsgBoxCallback(3);
        }
    }
    else {
        Manager::GetSingleton()->MsgBoxCallback(3);
    }
}