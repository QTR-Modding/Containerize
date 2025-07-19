#pragma once
#include "ClibUtil/singleton.hpp"
#include "CLibUtilsQTR/Animations.hpp"
#include "Hooks.h"

namespace Animations
{
	class MyAnimator : 
		public Animator,
		public clib_util::singleton::ISingleton<MyAnimator>
	{
        RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event,
                                              RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override {
			if (!HasAnim()) {
				if (const auto a_actor = a_event->holder->As<RE::Actor>()) {
					RemoveSink(a_actor);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
			if (!a_event || !a_event->holder->IsPlayerRef()) {
				return RE::BSEventNotifyControl::kContinue;
			}
			const bool playing_open = !opened && a_event->tag == "AnimObjLoad";
			const bool playing_close = opened && a_event->tag == "AnimObjectUnequip";
			if (playing_open || playing_close) {
				if (const auto a_node = Hooks::objectNode) {
					a_node->CullGeometry(playing_open);
					a_node->CullNode(playing_open);
				}
				if (playing_close) {
				    Hooks::objectNode.reset();
					Reset();
				}

				if (const auto a_actor = a_event->holder->As<RE::Actor>()) {
					RemoveSink(a_actor);
				}

				opened = !opened;
			}
			
			return RE::BSEventNotifyControl::kContinue;
		}

		void Reset() {
			open_anim = {};
			close_anim = {};
        }

		bool opened = false; // only for animations with anim object
		std::vector<Animation> open_anim;
		std::vector<Animation> close_anim;

		void RemoveSink(const RE::Actor* a_actor) {a_actor->RemoveAnimationGraphEventSink(this);}

		bool HasAnim() const {
			return !open_anim.empty() && !close_anim.empty();
		}

	public:

		void SetOpenAnim(const std::vector<Animation>& a_open_anim) {
			open_anim = a_open_anim;
		}

		void SetCloseAnim(const std::vector<Animation>& a_close_anim) {
			close_anim = a_close_anim;
		}

		void OpenBag() {
		    Add2Q(open_anim);
		}
		void CloseBag() {
		    Add2Q(close_anim);
		}

	    unsigned int GetOpenDuration() const {
			unsigned int res = 0;
		    for (const auto& anim : open_anim) {
				res += anim.t_wait_ms;
		    }
			return res;
        }

		unsigned int GetCloseDuration() const {
			unsigned int res = 0;
			for (const auto& anim : close_anim) {
				res += anim.t_wait_ms;
			}
			return res;
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

	template <typename T>
    // ReSharper disable once CppFunctionIsNotImplemented
    int SetUpPlayAnimation(T* a_real, bool is_worn);
}