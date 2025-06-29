#include "SkyPrompt.h"
#include "Manager.h"

using namespace SkyPrompt;

void MyPromptSink::ProcessEvent(SkyPromptAPI::PromptEvent event) const
{
	if (event.type) {
		return;
	}
	SkyPromptAPI::RemovePrompt(this,g_clientID);
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

void MyPromptSink::SetRef(const RefID a_refid) {
	return;
    for (auto& prompt : prompts) {
        prompt.refid = a_refid;
    }
}

void MyPromptSink::UnSetRef() {
    for (auto& prompt : prompts) {
        prompt.refid = 0;
    }
}
