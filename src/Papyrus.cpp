#include "Papyrus.h"
#include "Manager.h"

RE::VMHandle Papyrus::GetHandle(const RE::TESForm* a_form) {
    const auto vm = VM::GetSingleton();
    const auto policy = vm->GetObjectHandlePolicy();
    return policy->GetHandleForObject(a_form->GetFormType(), a_form);
}

Papyrus::ObjectPtr Papyrus::GetObjectPtr(const RE::TESForm* a_form, const char* a_class, const bool a_create) {
    const auto vm = VM::GetSingleton();
    const auto handle = GetHandle(a_form);

    ObjectPtr object = nullptr;
    if (const bool found = vm->FindBoundObject(handle, a_class, object); !found && a_create) {
        vm->CreateObject2(a_class, object);
        vm->BindObject(object, handle, false);
    }
    return object;
}

void Papyrus::ConversationCallbackFunctor::operator()(const RE::BSScript::Variable a_result) {
    if (a_result.IsNoneObject()) {
        logger::trace("Result: None");
    } else if (a_result.IsString()) {
        rename = a_result.GetString();
        logger::trace("Result rename: {}", rename);
        if (!rename.empty()) {
            Manager::GetSingleton()->RenameContainer(rename);
        }
    }
}

void Papyrus::RenameCallbackFunctor::OnRename() {
    logger::trace("Rename menu closed.");
    const auto skyrimVM = RE::SkyrimVM::GetSingleton();
    if (const auto vm = skyrimVM ? skyrimVM->impl : nullptr) {
        const char* menuID = "UITextEntryMenu";
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new ConversationCallbackFunctor());
        const auto args = RE::MakeFunctionArguments(std::move(menuID));
        if (!vm->DispatchStaticCall("UIExtensions", "GetMenuResultString", args, callback)) {
        }
    }
}