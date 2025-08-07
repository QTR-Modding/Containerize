#include "Animations.h"
#include "Manager.h"
#include "DynamicAnimationFramework/API.h"

namespace  {
    template <typename T>
    int SetUpPlayAnimation_Impl(T* a_real, bool is_worn) {
	    using namespace ModCompatibility::Mods;
	    auto manager = Manager::GetSingleton();

        int duration = 0;

        if (const auto player_cam = RE::PlayerCamera::GetSingleton();
            !manager->IsInChestMenu() && 
		    (!other_settings.at(Settings::otherstuffKeys.at(7)) || is_worn) && 
		    (player_cam->IsInThirdPerson() || player_cam->IsInFirstPerson() && improved_cam_path_installed)
		    ) 
	    {
		    if (souls_unpaused_installed || Settings::AnimationsDelayMenuOpen()) {
				duration = Animations::SendAnimEvent(true,a_real);
		    }

		    if (!Settings::AnimationsDelayMenuOpen()) {
			    duration = 0;
		    }
	    }
	    return duration;
    }
}

template <>
int Animations::SetUpPlayAnimation(RE::TESBoundObject* a_real, const bool is_worn)
{
	return SetUpPlayAnimation_Impl(a_real, is_worn);
}

template <>
int Animations::SetUpPlayAnimation(RE::TESObjectREFR* a_real, const bool is_worn)
{
	return SetUpPlayAnimation_Impl(a_real, is_worn);
}

int Animations::SendAnimEvent(bool open, RE::TESForm* a_real)
{
	auto& a_id = open ? anim_event_id_open : anim_event_id_close;
	const char* a_event = open ? anim_event_open : anim_event_close;

	if (a_id == 0) {
		a_id = DAF_API::RequestEventID(a_event);
	}

	if (a_id > 0) {
		RE::PlayerCharacter::GetSingleton()->AddAnimationGraphEventSink(Animations::MyAnimator::GetSingleton());
		return DAF_API::SendEvent(a_id,0x14,a_real ? a_real->GetFormID() : 0);
	}
	return 0;
}
