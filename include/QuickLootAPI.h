#pragma once

/*
    Header File for QuickLoot integration
*/

namespace QuickLoot::API {
    struct ItemStack {
        RE::InventoryEntryData* entry;
        RE::TESObjectREFR* dropRef;
    };

    namespace Events {
        enum class HandleResult : uint8_t { kContinue = 0, kStop = 1 };

        struct TakingItemEvent {
            RE::Actor* actor;
            RE::TESObjectREFR* container;
            const ItemStack* stack;
            HandleResult result = HandleResult::kContinue;
        };

        struct TakeItemEvent {
            RE::Actor* actor;
            RE::TESObjectREFR* container;
            const ItemStack* stack;
        };

        struct SelectItemEvent {
            RE::Actor* actor;
            RE::TESObjectREFR* container;
            const ItemStack* stack;
        };

        struct OpeningLootMenuEvent {
            RE::TESObjectREFR* container;
            HandleResult result = HandleResult::kContinue;
        };

        struct OpenLootMenuEvent {
            RE::TESObjectREFR* container;
        };

        struct CloseLootMenuEvent {
            RE::TESObjectREFR* container;
        };

        struct InvalidateLootMenuEvent {
            RE::TESObjectREFR* container;
            const ItemStack* stacks;
            size_t stackCount;
        };

        struct ContainerOverrideEvent {
            RE::TESObjectREFR* const reference;
            RE::TESObjectREFR* overrideContainer = nullptr;
            HandleResult result = HandleResult::kContinue;
        };

        template <typename TEvent>
        using EventHandler = void (*)(TEvent* e);

        template <typename TEvent>
        struct HandlerRegistrationRequest {
            EventHandler<TEvent> handler;
        };

        using TakingItemHandler = EventHandler<TakingItemEvent>;
        using TakeItemHandler = EventHandler<TakeItemEvent>;
        using SelectItemHandler = EventHandler<SelectItemEvent>;
        using OpeningLootMenuHandler = EventHandler<OpeningLootMenuEvent>;
        using OpenLootMenuHandler = EventHandler<OpenLootMenuEvent>;
        using CloseLootMenuHandler = EventHandler<CloseLootMenuEvent>;
        using InvalidateLootMenuHandler = EventHandler<InvalidateLootMenuEvent>;
        using ContainerOverrideHandler = EventHandler<ContainerOverrideEvent>;
    }

    using namespace Events;

    class QuickLootAPI {
    public:
        QuickLootAPI() = delete;
        ~QuickLootAPI() = delete;
        QuickLootAPI(QuickLootAPI const&) = delete;
        QuickLootAPI(QuickLootAPI const&&) = delete;
        QuickLootAPI operator=(QuickLootAPI&) = delete;
        QuickLootAPI operator=(QuickLootAPI&&) = delete;

        static constexpr const char* SERVER_PLUGIN_NAME = "QuickLootIE";

        // Call this before any other API function and pass your own plugin name.
        static bool Init(const char* plugin) {
            using GetInterfaceV21Proc = InterfaceV21* (*)();
            using GetInterfaceV20Proc = InterfaceV20* (*)();

            const auto dllHandle = GetModuleHandleA(SERVER_PLUGIN_NAME);
            const auto getV21 =
                reinterpret_cast<GetInterfaceV21Proc>(GetProcAddress(dllHandle, "GetQuickLootInterfaceV21"));
            const auto getV20 =
                reinterpret_cast<GetInterfaceV20Proc>(GetProcAddress(dllHandle, "GetQuickLootInterfaceV20"));

            if (getV21) {
                _plugin = plugin;
                _interfaceV21 = getV21();
                _interface = _interfaceV21;
            } else if (getV20) {
                _plugin = plugin;
                _interface = getV20();
                _interfaceV21 = nullptr;
            }

            return IsReady();
        }

        static bool IsReady() { return _interface; }
        static bool IsReadyV21() { return _interfaceV21 != nullptr; }

        static void DisableLootMenu() {
            if (_interface) {
                _interface->DisableLootMenu(_plugin);
            }
        }

        static void EnableLootMenu() {
            if (_interface) {
                _interface->EnableLootMenu(_plugin);
            }
        }

        static void RegisterTakingItemHandler(TakingItemHandler handler) {
            if (_interface) {
                _interface->RegisterTakingItemHandler(_plugin, handler);
            }
        }

        static void RegisterTakeItemHandler(TakeItemHandler handler) {
            if (_interface) {
                _interface->RegisterTakeItemHandler(_plugin, handler);
            }
        }

        static void RegisterSelectItemHandler(SelectItemHandler handler) {
            if (_interface) {
                _interface->RegisterSelectItemHandler(_plugin, handler);
            }
        }

        static void RegisterOpeningLootMenuHandler(OpeningLootMenuHandler handler) {
            if (_interface) {
                _interface->RegisterOpeningLootMenuHandler(_plugin, handler);
            }
        }

        static void RegisterOpenLootMenuHandler(OpenLootMenuHandler handler) {
            if (_interface) {
                _interface->RegisterOpenLootMenuHandler(_plugin, handler);
            }
        }

        static void RegisterCloseLootMenuHandler(CloseLootMenuHandler handler) {
            if (_interface) {
                _interface->RegisterCloseLootMenuHandler(_plugin, handler);
            }
        }

        static void RegisterInvalidateLootMenuHandler(InvalidateLootMenuHandler handler) {
            if (_interface) {
                _interface->RegisterInvalidateLootMenuHandler(_plugin, handler);
            }
        }

        static void RegisterContainerOverrideHandler(ContainerOverrideHandler handler) {
            if (_interfaceV21) {
                _interfaceV21->RegisterContainerOverrideHandler(_plugin, handler);
            }
        }

    private:
        // ReSharper disable once CppPolymorphicClassWithNonVirtualPublicDestructor
        struct InterfaceV20 {
            virtual void DisableLootMenu(const char* plugin);
            virtual void EnableLootMenu(const char* plugin);

            virtual void RegisterTakingItemHandler(const char* plugin, TakingItemHandler handler);
            virtual void RegisterTakeItemHandler(const char* plugin, TakeItemHandler handler);
            virtual void RegisterSelectItemHandler(const char* plugin, SelectItemHandler handler);
            virtual void RegisterOpeningLootMenuHandler(const char* plugin, OpeningLootMenuHandler handler);
            virtual void RegisterOpenLootMenuHandler(const char* plugin, OpenLootMenuHandler handler);
            virtual void RegisterCloseLootMenuHandler(const char* plugin, CloseLootMenuHandler handler);
            virtual void RegisterInvalidateLootMenuHandler(const char* plugin, InvalidateLootMenuHandler handler);
        };

        struct InterfaceV21 : InterfaceV20 {
            virtual void RegisterContainerOverrideHandler(const char* plugin, ContainerOverrideHandler handler);
        };

        static inline const char* _plugin;
        static inline InterfaceV20* _interface;
        static inline InterfaceV21* _interfaceV21 = nullptr;
    };
}
