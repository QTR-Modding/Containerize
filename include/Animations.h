#pragma once
#include "ClibUtil/singleton.hpp"
#include "CLibUtilsQTR/Animations.hpp"

namespace Animations
{
	class MyAnimator : 
		public Animator,
		public clib_util::singleton::ISingleton<MyAnimator>
	{
		Animation idlestop_anim = {.a_idle= nullptr, .anim_name= "IdleStop", .t_wait_ms= 2000};
        //Animation backpack_anim = {.idle= nullptr, .anim_name= "ImmersiveBackpackAnimation", .t_wait_ms= 3000};

        RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent*,
                                              RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override {
			return RE::BSEventNotifyControl::kContinue;
		}

		void Reset() {
			open_anim = Animation();
			close_anim = Animation();
        }
	public:

		Animation open_anim;
		Animation close_anim;

		void OpenBackpack() {
		    Add2Q({open_anim});
		}
		void CloseBackpack() {
		    Add2Q({close_anim});
			Reset();
		}
	};

	enum AnimDataType : uint8_t {
		kInventory = 0,
		kDrop = 1,
	};

	struct AnimData {
		Animation open;
		Animation close;
		std::string attach_node;
	};
}