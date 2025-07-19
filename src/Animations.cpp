#include "Manager.h"

namespace  {
    template <typename T>
    int SetUpPlayAnimation_Impl(T* a_real, bool is_worn) {
	    using namespace ModCompatibility::Mods;
	    auto manager = Manager::GetSingleton();
	    auto animator = Animations::MyAnimator::GetSingleton();

        int duration = 0;

        if (const auto player_cam = RE::PlayerCamera::GetSingleton();
            !manager->IsInChestMenu() && 
		    (!other_settings.at(Settings::otherstuffKeys.at(7)) || is_worn) && 
		    (player_cam->IsInThirdPerson() || player_cam->IsInFirstPerson() && improved_cam_path_installed)
		    ) 
	    {
		    if (souls_unpaused_installed || Settings::AnimationsDelayMenuOpen()) {
			    Hooks::container_mesh = a_real->GetFormID();
	            manager->SetUpAnimation(a_real);
		        animator->OpenBag();
		    }

		    if (Settings::AnimationsDelayMenuOpen()) {
			    duration = animator->GetOpenDuration();
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
