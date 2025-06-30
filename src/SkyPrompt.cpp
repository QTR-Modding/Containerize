#include "SkyPrompt.h"
#include "Events.h"
#include "Manager.h"

using namespace SkyPrompt;

void MyPromptSink::ProcessEvent(SkyPromptAPI::PromptEvent event) const
{
	if (event.type) {
		return;
	}
	EventSink::RemovePrompts();
	const auto M = Manager::GetSingleton();
	if (const auto crosshairref = RE::CrosshairPickData::GetSingleton()->target) {
        //if (const auto prompt_text = event.prompt.text; prompt_text == "Open") {
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
