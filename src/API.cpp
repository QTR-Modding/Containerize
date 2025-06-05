#include "API.h"
#include "Manager.h"

void MyPromptSink::ProcessEvent(const SkyPromptAPI::PromptEvent event)
{
	logger::info("SPS::ProcessEvent {}", event.type);

	if (event.type) {
		return;
	}

	if (const auto crosshairref = RE::CrosshairPickData::GetSingleton()->target) {
        //if (const auto prompt_text = event.prompt.text; prompt_text == "Open") {
        if (const auto prompt_eventid = event.prompt.eventID; prompt_eventid == 0) {
		    M->OnActivateContainer(crosshairref.get().get(),false);
		    M->OpenContainer();
			for (int i = 0; i < 30; ++i) {
			    SkyPromptAPI::RemovePrompt(this,g_clientID);
			    if (!SkyPromptAPI::SendPrompt(this,g_clientID)) {
				    logger::warn("Prompt event: Failed to send prompt.");
			    }
			}
	    }
	    //else if (prompt_text == "Take") {
	    else if (prompt_eventid == 1) {
		    //logger::info("Prompt event: Take");
		    M->OnActivateContainer(crosshairref.get().get(),false);
		    M->TakeContainer();
			SkyPromptAPI::RemovePrompt(this,g_clientID);
	    }
	    //else if (prompt_text == "Rename") {
	    else if (prompt_eventid == 2) {
		    //logger::info("Prompt event: Rename");
		    M->OnActivateContainer(crosshairref.get().get(),false);
		    M->OpenRenameContainer();
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
	int i = 0;
    for (auto& prompt : prompts) {
		if (i==3) continue;
        prompt.refid = a_refid;
		++i;
    }
}

void MyPromptSink::UnSetRef() {
    for (auto& prompt : prompts) {
        prompt.refid = 0;
    }
}
