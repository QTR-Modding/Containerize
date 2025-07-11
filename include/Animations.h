#pragma once
#include "ClibUtil/singleton.hpp"
#include "CLibUtilsQTR/Animations.hpp"

namespace Animations
{
	class MyAnimator : 
		public Animator,
		public clib_util::singleton::ISingleton<MyAnimator>
	{
		Animation idlestop_anim = {.a_idle= nullptr, .anim_name= "IdleStop", .t_wait_ms= 3000};
        Animation backpack_anim = {.a_idle= nullptr, .anim_name= "ImmersiveBackpackAnimation", .t_wait_ms= 3000};

        RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent*,
                                              RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override {
			return RE::BSEventNotifyControl::kContinue;
		}
	public:
		void OpenBackpack() {
		    Add2Q({idlestop_anim,backpack_anim});
		}
		void CloseBackpack() {
		    Add2Q({idlestop_anim});
		}
	};
}