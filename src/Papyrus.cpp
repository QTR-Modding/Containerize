#include "Papyrus.h"
#include "MainManager.h"

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
    } else if (a_result.IsString()) {
        rename = a_result.GetString();
        if (!rename.empty()) {
            FormID target_id;
            if (const auto handle = std::get_if<RE::ObjectRefHandle>(&target)) {
                const auto reference = handle->get();
                if (!reference) return;
                target_id = reference->GetFormID();
            } else {
                target_id = std::get<RE::TESBoundObject*>(target)->GetFormID();
            }
            if (!MainManager::GetSingleton()->Rename(target_id, rename.c_str())) {
                logger::warn("Failed to rename container {:x}", target_id);
            }
        }
    } else {
        logger::info("a_result type {}", a_result.GetType().TypeAsString().c_str());
    }
}

void Papyrus::RenameCallbackFunctor::OnRename() {
    const auto smart = RE::make_smart<ConversationCallbackFunctor>(target);
    if (!CallFunction("UIExtensions", "GetMenuResultString", smart.get(), "UITextEntryMenu")) {
        logger::error("Failed to get menu result string from UIExtensions.");
    }
}