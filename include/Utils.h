#pragma once
#include <windows.h>
#include "ClibUtil/editorID.hpp"
#include "SimpleIni.h"
#include "CLibUtilsQTR/FormReader.hpp"


bool GetDllVersion(const std::wstring& dllPath, DWORD& major, DWORD& minor, DWORD& build, DWORD& revision);
std::wstring s2ws(const std::string& str);

const auto mod_name = static_cast<std::string>(SKSE::PluginDeclaration::GetSingleton()->GetName());

namespace ModCompatibility {
    inline bool IsModInstalled(const char* mod_path) {return std::filesystem::exists(mod_path);}
    namespace Mods {

        constexpr auto po3path = "Data/SKSE/Plugins/po3_Tweaks.dll";
        bool IsPo3Installed();

        constexpr auto po3_UoTpath = "Data/SKSE/Plugins/po3_UseOrTake.dll";
		const auto po3_use_or_take = IsModInstalled(po3_UoTpath);

        constexpr auto obj_manipu_path = "Data/SKSE/Plugins/ObjectManipulationOverhaul.dll";
		const auto obj_manipu_installed = IsModInstalled(obj_manipu_path);

        constexpr auto souls_unpaused_path = "Data/SKSE/Plugins/SkyrimSoulsRE.dll";
		const auto souls_unpaused_installed = IsModInstalled(souls_unpaused_path);

        constexpr auto improved_cam_path = "Data/SKSE/Plugins/ImprovedCameraSE.dll";
		const auto improved_cam_path_installed = IsModInstalled(improved_cam_path);

        constexpr auto ui_extensions_path = "UIExtensions.esp";
		inline bool ui_extensions_installed = false;

        // CC content
        constexpr auto doppelgangers_path = "ccbgssse018-shadowrend.esl";
        inline const std::set<FormID> doppelgangers_local = {0x832,0x833,0x834,0x835,0x836,0x837,0x838,0x839,0x83a,0x83b};
        inline std::set<FormID> doppelgangers;
    }

    void MakeChecks();
    void Load();
}

namespace UnownedStuff {
    // unowned stuff
    constexpr RefID unownedChestOGRefID = 0x000EA29A;
    constexpr RefID unownedChestFormID = 0x000EA299;
    //RE::TESObjectCELL* unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000FE47B);  // cwquartermastercontainers
    //RE::TESObjectCONT* unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(0x000A0DB5); // playerhousechestnew
    constexpr RE::NiPoint3 unownedChestPos = {1986.f, 1780.f, 6784.f};
}


inline std::string no_src_msgbox = std::format(
    "{}: You currently do not have any container set up. Check your ini file or see the mod page for instructions.",
    mod_name);

inline std::string po3_err_msgbox = std::format(
    "{}: You must have the latest version of powerofthree's Tweaks "
    "installed. See mod page for further instructions.",
    mod_name);

inline std::string general_err_msgbox = "Something went wrong. Please contact the mod author.";
inline std::string init_err_msgbox = "The mod failed to initialize and will be terminated.";
inline std::string form_type_err_msgbox = "The form type of the item with FormID ({:x}) is not supported. Please contact the mod author.";
inline std::string uninstall_msgbox = "Uninstall successful. You can now remove the mod. Please save and quit the game.";
inline std::string uninstall_err_msgbox = "Uninstall failed. Please contact the mod author.";
inline std::string problem_with_container_msgbox = "Problem with one of the items with the form id ({:x}). This is expected if you have changed the list of containers in the INI file between saves. Corresponding items will be returned to your inventory. You can suppress this message by changing the setting in your INI.";
inline std::vector<std::string> buttons = {"Open", "Take", "More...", "Close"};
inline std::vector<std::string> buttons_more = {"Rename", "Uninstall", "Back", "Close"};

void SetupLog();
std::filesystem::path GetLogPath();
std::vector<std::string> ReadLogFile();

std::string DecodeTypeCode(std::uint32_t typeCode);

std::string GetGameLanguage();

namespace Functions {

    template <typename Key, typename Value>
    bool containsValue(const std::map<Key, Value>& myMap, const Value& valueToFind) {
        for (const auto& pair : myMap) {
            if (pair.second == valueToFind) {
                return true;
            }
        }
        return false;
    }

    template <typename Key, typename Value>
    void printMap(const std::map<Key, Value>& myMap) {
        for (const auto& pair : myMap) {
			logger::trace("Key: {}, Value: {}", pair.first, pair.second);
		}
	}
}

namespace Math {

    /*float Round(float value, int n);
    float Ceil(float value, int n);*/

    namespace LinAlg {
        namespace R3 {
            void rotateX(RE::NiPoint3& v, float angle);

            // Function to rotate a vector around the y-axis
            void rotateY(RE::NiPoint3& v, float angle);

            // Function to rotate a vector around the z-axis
            void rotateZ(RE::NiPoint3& v, float angle);

            void rotate(RE::NiPoint3& v, float angleX, float angleY, float angleZ);
        };
    };
};



namespace FunctionsSkyrim {

    template <typename T>
    struct FormTraits {
        static float GetWeight(T* form) {
            // Default implementation, assuming T has a member variable 'weight'
            return form->weight;
        }

        static void SetWeight(T* form, float weight) {
            // Default implementation, set the weight if T has a member variable 'weight'
            form->weight = weight;
        }

        static int GetValue(T* form) {
			// Default implementation, assuming T has a member variable 'value'
			return form->value;
		}

        static void SetValue(T* form, int value) {
            form->value = value;
        }
    };

    // Specialization for TESAmmo
    template <>
    struct FormTraits<RE::TESAmmo> {
        static float GetWeight(RE::TESAmmo*) {
            // Handle TESAmmo case where 'weight' is not a member
            // You might return a default value or calculate it based on other factors
            return 0.0f;  // For example, returning 0 as a default value
        }

        static void SetWeight(RE::TESAmmo*, float) {
            // Handle setting the weight for TESAmmo
            // (implementation based on your requirements)
            // For example, if TESAmmo had a SetWeight method, you would call it here
        }

        static int GetValue(RE::TESAmmo* form) {
			return form->value;
		}
        static void SetValue(RE::TESAmmo* form, const int value) {
			form->value = value;
		}
    };

    template <>
    struct FormTraits<RE::AlchemyItem> {
        static float GetWeight(RE::AlchemyItem* form) { 
            return form->weight;
        }

        static void SetWeight(RE::AlchemyItem* form, const float weight) { 
            form->weight = weight;
        }

        static int GetValue(RE::AlchemyItem* form) {
        	return form->GetGoldValue();
        }
        static void SetValue(RE::AlchemyItem* form, const int value) { 
            logger::trace("CostOverride: {}", form->data.costOverride);
            form->data.costOverride = value;
        }
    };

}

namespace MsgBoxesNotifs {

    // https://github.com/SkyrimScripting/MessageBox/blob/ac0ea32af02766582209e784689eb0dd7d731d57/include/SkyrimScripting/MessageBox.h#L9
    class SkyrimMessageBox {
        class MessageBoxResultCallback : public RE::IMessageBoxCallback {
            std::function<void(unsigned int)> _callback;

        public:
            ~MessageBoxResultCallback() override = default;
            explicit MessageBoxResultCallback(std::function<void(unsigned int)> callback) : _callback(callback) {}
            void Run(RE::IMessageBoxCallback::Message message) override {
                _callback(static_cast<unsigned int>(message));
            }
        };

    public:
        static void Show(const std::string& bodyText, const std::vector<std::string>& buttonTextValues,
                         std::function<void(unsigned int)> callback);
    };

    inline void ShowMessageBox(const std::string& bodyText, const std::vector<std::string>& buttonTextValues,
                               const std::function<void(unsigned int)>& callback) {
        SkyrimMessageBox::Show(bodyText, buttonTextValues, callback);
    }

    namespace Windows {

        inline int Po3ErrMsg() {
            MessageBoxA(nullptr, po3_err_msgbox.c_str(), "Error", MB_OK | MB_ICONERROR);
            return 1;
        };
    };

    namespace InGame{
        inline void CustomMsg(const std::string& msg) { RE::DebugMessageBox((mod_name + ": " + msg).c_str()); };
		inline void GeneralErr() {
            const std::string message = std::format("{}: {}", mod_name, general_err_msgbox);
            RE::DebugMessageBox(message.c_str());
        }

		inline void InitErr() {
			const std::string message = std::format("{}: {}", mod_name, init_err_msgbox);
			RE::DebugMessageBox(message.c_str());
		}

		inline void FormTypeErr(RE::FormID id) {
            std::string message = std::format("{}: {}", mod_name, form_type_err_msgbox);
            message = fmt::vformat(message, fmt::make_format_args(id));
            RE::DebugMessageBox(message.c_str());
        }

        inline void UninstallSuccessful() {
            const std::string message = fmt::vformat("{}: {}", fmt::make_format_args(mod_name, uninstall_msgbox));
            RE::DebugMessageBox(message.c_str());
        }

        inline void UninstallFailed() {
            const std::string message = fmt::vformat("{}: {}", fmt::make_format_args(mod_name, uninstall_err_msgbox));
            RE::DebugMessageBox(message.c_str());
        }

        inline void ProblemWithContainer(RE::FormID id) {
            std::string message = fmt::vformat("{}: {}", fmt::make_format_args(mod_name, problem_with_container_msgbox));
            message = fmt::vformat(message, fmt::make_format_args(id));
            RE::DebugMessageBox(message.c_str());
        }


    };
    
};

namespace Inventory {
    bool EntryHasXDataList(const RE::InventoryEntryData* entry);

    inline bool HasItemEntry(RE::TESBoundObject* item, const RE::TESObjectREFR::InventoryItemMap& inventory,
                             bool nonzero_entry_check = false);

    inline bool HasItem(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner) {
        return HasItemEntry(item, inventory_owner->GetInventory(), true);
    }

    std::int32_t GetItemCount(RE::TESBoundObject* item, const RE::TESObjectREFR::InventoryItemMap& inventory);

    std::int32_t GetItemValue(RE::TESBoundObject* item,
                              const RE::TESObjectREFR::InventoryItemMap& inventory);

    bool IsQuestItem(FormID formid, RE::TESObjectREFR* inv_owner);

    inline int32_t GetEntryCostOverride(const RE::InventoryEntryData* entry);

    int GetValueInContainer(RE::TESObjectREFR* container);

    void FavoriteItem(const RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner);

    bool IsFavorited(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner);

    void EquipItem(const RE::TESBoundObject* item, bool unequip = false);

    inline void EquipItem(const FormID formid, const bool unequip = false) {
	    EquipItem(FormReader::GetFormByID<RE::TESBoundObject>(formid), unequip);
    }

    [[nodiscard]] bool IsEquipped(RE::TESBoundObject* item);

    [[nodiscard]] inline bool IsEquipped(const FormID formid) {
	    return IsEquipped(FormReader::GetFormByID<RE::TESBoundObject>(formid));
    }

    void ToggleEquip(RE::TESBoundObject* item);

};

namespace WorldObject {

    RE::TESObjectREFR* DropObjectIntoTheWorld(RE::TESBoundObject* obj, Count count=1, bool player_owned=true);

    void SwapObjects(RE::TESObjectREFR* a_from, RE::TESBoundObject* a_to, bool apply_havok=true);

    inline void StartDraggingObject(RE::TESObjectREFR* ref) {
        using func_t = void(*)(RE::TESObjectREFR*);
        static auto ObjectManipulationOverhaul = GetModuleHandle(L"ObjectManipulationOverhaul");
        const auto func = reinterpret_cast<func_t>(GetProcAddress(ObjectManipulationOverhaul, "StartDraggingObject"));  // NOLINT(clang-diagnostic-cast-function-type-strict)
        return func(ref);
    }
};

namespace xData {

    namespace Copy {
        void CopyEnchantment(const RE::ExtraEnchantment* from, RE::ExtraEnchantment* to);
            
        void CopyHealth(const RE::ExtraHealth* from, RE::ExtraHealth* to);

        void CopyRank(const RE::ExtraRank* from, RE::ExtraRank* to);

        void CopyTimeLeft(const RE::ExtraTimeLeft* from, RE::ExtraTimeLeft* to);

        void CopyCharge(const RE::ExtraCharge* from, RE::ExtraCharge* to);

        void CopyScale(const RE::ExtraScale* from, RE::ExtraScale* to);

        void CopyUniqueID(const RE::ExtraUniqueID* from, RE::ExtraUniqueID* to);

        void CopyPoison(const RE::ExtraPoison* from, RE::ExtraPoison* to);

        void CopyObjectHealth(const RE::ExtraObjectHealth* from, RE::ExtraObjectHealth* to);

        void CopyLight(const RE::ExtraLight* from, RE::ExtraLight* to);

        void CopyRadius(const RE::ExtraRadius* from, RE::ExtraRadius* to);

        void CopyHorse(const RE::ExtraHorse* from, RE::ExtraHorse* to);

        void CopyHotkey(const RE::ExtraHotkey* from, RE::ExtraHotkey* to);

        void CopyTextDisplayData(const RE::ExtraTextDisplayData* from, RE::ExtraTextDisplayData* to);

        void CopySoul(const RE::ExtraSoul* from, RE::ExtraSoul* to);

        void CopyOwnership(const RE::ExtraOwnership* from, RE::ExtraOwnership* to);
    };

    template <typename T>
    void CopyExtraData(T* from, T* to){
        if (!from || !to) return;
        switch (T::EXTRADATATYPE) {
            case RE::ExtraDataType::kEnchantment:
                CopyEnchantment(from, to);
                break;
            case RE::ExtraDataType::kHealth:
                CopyHealth(from, to);
                break;
            case RE::ExtraDataType::kRank:
                CopyRank(from, to);
                break;
            case RE::ExtraDataType::kTimeLeft:
                CopyTimeLeft(from, to);
                break;
            case RE::ExtraDataType::kCharge:
                CopyCharge(from, to);
                break;
            case RE::ExtraDataType::kScale:
                CopyScale(from, to);
                break;
            case RE::ExtraDataType::kUniqueID:
                CopyUniqueID(from, to);
                break;
            case RE::ExtraDataType::kPoison:
                CopyPoison(from, to);
                break;
            case RE::ExtraDataType::kObjectHealth:
                CopyObjectHealth(from, to);
                break;
            case RE::ExtraDataType::kLight:
                CopyLight(from, to);
                break;
            case RE::ExtraDataType::kRadius:
                CopyRadius(from, to);
                break;
            case RE::ExtraDataType::kHorse:
                CopyHorse(from, to);
				break;
            case RE::ExtraDataType::kHotkey:
                CopyHotkey(from, to);
				break;
            case RE::ExtraDataType::kTextDisplayData:
				CopyTextDisplayData(from, to);
				break;
            case RE::ExtraDataType::kSoul:
				CopySoul(from, to);
                break;
            case RE::ExtraDataType::kOwnership:
                CopyOwnership(from, to);
                break;
            default:
                logger::warn("ExtraData type not found");
                break;
        }
    }

    [[nodiscard]] bool UpdateExtras(RE::ExtraDataList* copy_from, RE::ExtraDataList* copy_to);

    [[nodiscard]] bool UpdateExtras(RE::TESObjectREFR* copy_from, RE::TESObjectREFR* copy_to);

    int32_t GetXDataCostOverride(const RE::ExtraDataList* xList);

    void AddTextDisplayData(RE::ExtraDataList* extraDataList, const std::string& displayName);

};

namespace DynamicForm {

    void copyBookAppearence(RE::TESForm* source, RE::TESForm* target);

    template <class T>
    static void copyComponent(RE::TESForm* from, RE::TESForm* to) {
        auto fromT = from->As<T>();

        auto toT = to->As<T>();

        if (fromT && toT) {
            toT->CopyComponent(fromT);
        }
    }

    void copyFormArmorModel(RE::TESForm* source, RE::TESForm* target);

    void copyFormObjectWeaponModel(RE::TESForm* source, RE::TESForm* target);

    void copyMagicEffect(RE::TESForm* source, RE::TESForm* target);

    void copyAppearence(RE::TESForm* source, RE::TESForm* target);

};

namespace Menu {
    std::string_view CloseMenu();

    bool IsOpen(const RE::BSFixedString& menu_name);

    void OpenMenu(std::string_view menuname);

	bool GetContainerMenuOwner(RE::TESObjectREFRPtr& a_out);
};


class SpeedProfiler {
	std::chrono::time_point<std::chrono::steady_clock> start_time;
	std::chrono::time_point<std::chrono::steady_clock> end_time;
	std::string name;
public:
    explicit SpeedProfiler(const std::string& name) {
		start_time = std::chrono::steady_clock::now();
		this->name = name;
	}
	~SpeedProfiler() {
		end_time = std::chrono::steady_clock::now();
		std::chrono::duration<double> elapsed_seconds = end_time - start_time;
		logger::info("{}: Elapsed time: {}", name, elapsed_seconds.count());
	}
};