#include "SkyPrompt.h"
#include "Events.h"
#include "Hooks.h"
#include "Manager.h"

using namespace SkyPrompt;

void MyPromptSink::ProcessEvent(const SkyPromptAPI::PromptEvent event) const
{
	if (event.type) {
		return;
	}
	EventSink::RemovePrompts();
	const auto M = Manager::GetSingleton();
	if (const auto crosshairref = RE::CrosshairPickData::GetSingleton()->target) {
        if (const auto prompt_eventid = event.prompt.eventID; 
			prompt_eventid == 0) {
		    M->OnActivateContainer(crosshairref.get().get(), 0);
	    }
	    else if (prompt_eventid == 1) {
		    //logger::info("Prompt event: Rename");
		    M->OnActivateContainer(crosshairref.get().get(),2);
	    }
	    else {
		    logger::warn("Prompt event: Unrecognized prompt.");
	    }
	}
	else {
		logger::warn("Crosshair ref is null.");
	}
}

void MyPromptSink2::Start(const RE::TESObjectREFR* a_ref) {
	weight_text.clear();
	value_text.clear();

    const auto manager = Manager::GetSingleton();
	const auto a_weight_text = manager->GetWeightText(a_ref);

    const auto a_value_text = manager->GetValueText(a_ref);
	if (a_weight_text.empty() || a_value_text.empty()) {
		return;
	}

	weight_text.append("W: ").append(a_weight_text);
	value_text.append("V: ").append(a_value_text);

	weight_prompt.text = weight_text;
	value_prompt.text = value_text;

    prompts = { weight_prompt, value_prompt };
}

bool SkyPrompt::IsAnyMenuOpen() {
    const auto ui = RE::UI::GetSingleton();
    for (const auto a_name : blockedMenus) {
        if (ui->IsMenuOpen(a_name)) {
            return true;
        }
    }
	return false;
}

void SkyPrompt::MenuPromptSink::ProcessEvent(const SkyPromptAPI::PromptEvent event) const
{
	if (event.type) {
		return;
	}
	if (const auto a_item = Hooks::GetSelectedItemInMenu()){
        if (const auto a_formid = a_item->GetFormID(); 
			!Hooks::container_meshes.contains(a_formid)) {
            for (const auto& loaded_models = RE::Inventory3DManager::GetSingleton()->GetRuntimeData().loadedModels; 
				auto& a_loaded_model : loaded_models) {
			    if (a_loaded_model.modelObj->GetFormID() == a_formid) {
                    Hooks::container_meshes[a_formid] = a_loaded_model.spModel;
					break;
			    }
		    }
	    }
	    Manager::GetSingleton()->OnLongPressEquip(a_item);
	}
}

void SkyPrompt::MenuPromptSink::Show(RE::TESBoundObject* a_item) const {

    weight_text.clear();

	if (const auto a_ref = RE::Inventory3DManager::GetSingleton()->tempRef) {
		const auto refid = a_ref->GetFormID();
		open_prompt.refid = refid;
		weight_prompt.refid = refid;

		const auto a_weight_text = Manager::GetSingleton()->GetWeightText(a_item);
		weight_text.append("W: ").append(a_weight_text);
		weight_prompt.text = weight_text;
	}
	else {
		open_prompt.refid = 0;
		weight_prompt.refid = 0;
	}
	prompts = { open_prompt,weight_prompt};

	if (!SkyPromptAPI::SendPrompt(this,g_clientID)) {
	}
}

void SkyPrompt::MenuPromptSink::Hide() const {
	SkyPromptAPI::RemovePrompt(this,g_clientID);
}
