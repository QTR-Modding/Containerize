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
        logger::warn("Result: None");
    }
    else if (a_result.IsString()) {
        rename = a_result.GetString();
        if (!rename.empty()) {
            Manager::GetSingleton()->RenameContainer(rename,fake);
        }
    }
    else {
        logger::info("a_result type {}", a_result.GetType().TypeAsString().c_str());
    }
}

void Papyrus::RenameCallbackFunctor::OnRename() {
    const auto smart = RE::make_smart<ConversationCallbackFunctor>(fake);
    if (!CallFunction("UIExtensions","GetMenuResultString",smart.get(),"UITextEntryMenu")) {
		logger::error("Failed to get menu result string from UIExtensions.");
    }
    
}