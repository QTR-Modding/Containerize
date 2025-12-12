#pragma once
#include <REX/REX/Singleton.h>
#include "Hooks.h"
#include "DynamicAnimationFramework/API.hpp"

namespace Animations
{
	inline DAF_API::AnimEventID anim_event_id_open = 0;
	inline DAF_API::AnimEventID anim_event_id_open_world = 0;
	inline DAF_API::AnimEventID anim_event_id_close = 0;
	inline DAF_API::AnimEventID anim_event_id_close_world = 0;

    inline const char* anim_event_open = "ContainerizeOpen";
	inline const char* anim_event_open_world = "ContainerizeOpenWorld";
    inline const char* anim_event_close = "ContainerizeClose";
	inline const char* anim_event_close_world = "ContainerizeCloseWorld";

	int SendAnimEvent(bool open, const RE::TESForm* a_real);

	class AnimSink : 
		public RE::BSTEventSink<RE::BSAnimationGraphEvent>,
		public REX::Singleton<AnimSink>
	{
        RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event,
                                              RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override {

			const bool playing_open = !opened && a_event->tag == "AnimObjLoad";
			const bool playing_close = opened && a_event->tag == "AnimObjectUnequip";
			if (playing_open || playing_close) {
				if (const auto a_node = Hooks::objectNode) {
					a_node->CullGeometry(playing_open);
					a_node->CullNode(playing_open);
				}
				if (playing_close) {
				    Hooks::objectNode.reset();
				    if (const auto a_actor = a_event->holder->As<RE::Actor>()) {
					    RemoveSink(a_actor);
				    }
				}


				opened = !opened;
			}
			
			return RE::BSEventNotifyControl::kContinue;
		}

		bool opened = false; // only for animations with anim object

		void RemoveSink(const RE::Actor* a_actor) {a_actor->RemoveAnimationGraphEventSink(this);}
	};

	template <typename T>
    // ReSharper disable once CppFunctionIsNotImplemented
    int SetUpPlayAnimation(T* a_real, bool is_worn);
}