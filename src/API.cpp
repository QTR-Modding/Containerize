#include "API.h"
#include "Manager.h"

void MyPromptSink::ProcessEvent(const SkyPromptAPI::PromptEvent event)
{
	if (event.type) {
		return;
	}

	if (const auto crosshairref = RE::CrosshairPickData::GetSingleton()->target) {
        if (const auto prompt_text = event.prompt.text; prompt_text == "Open") {
		    M->OnActivateContainer(crosshairref.get().get(),false);
		    M->OpenContainer();
			if (!SkyPromptAPI::SendPrompt(this,true)) {
				logger::warn("Prompt event: Failed to send prompt.");
			}
	    }
	    else if (prompt_text == "Take") {
		    logger::info("Prompt event: Take");
		    M->OnActivateContainer(crosshairref.get().get(),false);
		    M->TakeContainer();
			SkyPromptAPI::RemovePrompt(this);
	    }
	    else if (prompt_text == "Rename") {
		    logger::info("Prompt event: Rename");
		    M->OnActivateContainer(crosshairref.get().get(),false);
		    M->OpenRenameContainer();
			if (!SkyPromptAPI::SendPrompt(this,true)) {
				logger::warn("Prompt event: Failed to send prompt.");
			}
	    }
	    else {
		    logger::warn("Prompt event: Unrecognized prompt.");
	    }
	}
	else {
		logger::warn("Crosshair ref is null.");
	}
}
