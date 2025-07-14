#pragma once
#include "ClibUtil/singleton.hpp"
#include "CLibUtilsQTR/Animations.hpp"

namespace Animations
{
	class MyAnimator : 
		public Animator,
		public clib_util::singleton::ISingleton<MyAnimator>
	{
        RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent*,
                                              RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override {
			return RE::BSEventNotifyControl::kContinue;
		}

		void Reset() {
			open_anim = {};
			close_anim = {};
        }
	public:

		std::vector<Animation> open_anim;
		std::vector<Animation> close_anim;

		void OpenBackpack() {
		    Add2Q(open_anim);
		}
		void CloseBackpack() {
		    Add2Q(close_anim);
			Reset();
		}
	};

	enum AnimDataType : uint8_t {
		kInventory = 0,
		kDrop = 1,
	};

	struct AnimData {
		std::vector<Animation> open;
		std::vector<Animation> close;
		std::string attach_node;
	};
}