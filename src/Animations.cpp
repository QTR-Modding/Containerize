#include "Animations.h"
#include "Manager.h"

namespace {
    template <typename T>
    int SetUpAnimationOnOpen_Impl(T* a_real, const bool is_worn, int animID_index) {
        if (REL::Module::IsVR()) {
            return 0;
        }
        using namespace ModCompatibility::Mods;
        const auto manager = Manager::GetSingleton();

        int duration = 0;

        const bool play_only_if_equipped = Settings::other_settings.at(Settings::otherstuffKeys.at(7));
        const bool is_in_third_person =
            RE::PlayerCharacter::GetSingleton()->GetPlayerRuntimeData().playerFlags.isInThirdPersonMode;
        if (!manager->IsInChestMenu() && (!play_only_if_equipped || is_worn) &&
            (is_in_third_person || improved_cam_path_installed)) {
            const bool should_delay = Settings::AnimationsDelayMenuOpen();
            if (souls_unpaused_installed || should_delay) {
                duration = Animations::SendAnimEvent(animID_index, a_real);
                duration = should_delay ? duration : 0;
            }
        }
        return duration;
    }
}

template <>
int Animations::SetUpAnimationOnOpen(RE::TESBoundObject* a_real, const bool is_worn) {
    return SetUpAnimationOnOpen_Impl(a_real, is_worn, 0);
}

template <>
int Animations::SetUpAnimationOnOpen(RE::TESObjectREFR* a_real, const bool is_worn) {
    return SetUpAnimationOnOpen_Impl(a_real, is_worn, 2);
}

int Animations::SendAnimEvent(const int animIDindex, const RE::TESForm* a_form) {
    if (REL::Module::IsVR()) {
        return 0;
    }
    auto a_id = anim_ids[animIDindex];
    if (a_id == 0) {
        a_id = DAF_API::RequestEventID(anim_events[animIDindex].data());
        anim_ids[animIDindex] = a_id;
    }
    if (a_id > 0) {
        RE::PlayerCharacter::GetSingleton()->AddAnimationGraphEventSink(AnimSink::GetSingleton());
        return DAF_API::SendEvent(a_id, 0x14, a_form ? a_form->GetFormID() : 0);
    }
    return 0;
}