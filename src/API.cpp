#include "API.h"
#include "Manager.h"

void MyPromptSink::ProcessEvent(const StreamlinedAPI::PromptEvent event)
{
	if (event.type) {
		return;
	}

	if (const auto crosshairref = RE::CrosshairPickData::GetSingleton()->target) {
        if (const std::string prompt_text = event.prompt.text; prompt_text == "Open") {
		    logger::info("Prompt event: Open");
		    M->OnActivateContainer(crosshairref.get().get(),false);
		    M->OpenContainer();
			if (!StreamlinedAPI::SendPrompt(this,true)) {
				logger::warn("Prompt event: Failed to send prompt.");
			}
	    }
	    else if (prompt_text == "Take") {
		    logger::info("Prompt event: Take");
		    M->OnActivateContainer(crosshairref.get().get(),false);
		    M->TakeContainer();
			StreamlinedAPI::RemovePrompt(this);
	    }
	    else if (prompt_text == "Rename") {
		    logger::info("Prompt event: Rename");
		    M->OnActivateContainer(crosshairref.get().get(),false);
		    M->OpenRenameContainer();
			if (!StreamlinedAPI::SendPrompt(this,true)) {
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
