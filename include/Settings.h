#pragma once
#include "CustomObjects.h"
#include <yaml-cpp/yaml.h>
#include <unordered_set>

namespace Settings {
    constexpr auto path = L"Data/SKSE/Plugins/Containerize.ini";

    //constexpr std::uint32_t kSerializationVersion = 729; // < 0.7
    //constexpr std::uint32_t kSerializationVersion = 730; // = 0.7.0
    //constexpr std::uint32_t kSerializationVersion = 731; // >= 0.7.1
    constexpr std::uint32_t kSerializationVersion = 732; // >= 0.10.0
    constexpr std::uint32_t kDataKey = 'CTRZ';
    constexpr std::uint32_t kDFDataKey = 'DCTZ';

    inline bool is_pre_0_7_1 = false;
    inline bool is_pre_0_10_0 = false;

    inline bool po3installed = false;

    constexpr std::array InISections = {"Containers",
                                        "Capacities",
                                        "Other Stuff"};

    constexpr std::array InIDefaultKeys = {"container1", "container1"};
    constexpr std::array InIDefaultVals = {"", ""};

    const std::array<std::string, 3> section_comments =
    {";Make sure to use unique keys, e.g. src1=... NOTsrc1=...",
     std::format(";Make sure to use matching keys with the ones provided in section {}.",
                 static_cast<std::string>(InISections[0])),
     ";Other gameplay related settings",
    };


    constexpr size_t otherstuffSize = 8;
    const std::array<std::string, otherstuffSize> os_comments =
    {";Set to false to suppress the 'INI changed between saves' message.",
     "; Set to true to remove the initial carry weight bonuses on your container items.",
     "; Set to true to return to the initial menu after closing your container's menu (which you had opened by holding equip).",
     "; BatchSell always enabled. (Ignored)",
     "; Set to true to make your containers weigh nothing by default.",
     "; Set to true to make use of Object Manipulation Overhaul upon dropping containers.",
     "; Set to true to delay the opening of the container menu until the animations are finished.",
     "; Set to true to play animations only if the Containerized item is equipped."
    };

    // BatchSell (index 3) kept for internal indexing but always true and removed from INI/menu.
    constexpr std::array<const char*, otherstuffSize> otherstuffKeys =
    {"INI_changed_msg", "RemoveCarryBoosts", "ReturnToInitialMenu", "BatchSell", "CloudStorage",
     "ObjectManipulationOverhaul", "AnimationsDelayMenuOpen", "PlayAnimationsOnlyIfEquipped"};
    constexpr std::array<bool, otherstuffSize> otherstuffVals = {true, true, true, true, false, false, false, false};

    inline bool cloud_storage_enabled = otherstuffVals[4];

    const std::unordered_set<std::string> AllowedFormTypes{
        "SCRL", //	17 SCRL	ScrollItem
        "ARMO", //	1A ARMO	TESObjectARMO
        "BOOK", //	1B BOOK	TESObjectBOOK
        "INGR", //	1E INGR	IngredientItem
        "MISC", //	20 MISC TESObjectMISC
        "WEAP", //	29 WEAP	TESObjectWEAP
        //"AMMO",  //	2A AMMO	TESAmmo
        "SLGM", // 34 SLGM	TESSoulGem
        "ALCH", //	2E ALCH	AlchemyItem
    };

    inline bool problems_in_YAML_sources = false;
    inline bool problems_in_INI_sources = false;
    inline bool duplicate_sources = false;

    bool AnimationsDelayMenuOpen();
};

std::vector<Source> LoadSources();
void LoadOtherSettings();
std::vector<Source> parseSources(const YAML::Node& config);
void LoadFormGroups();
std::vector<Source> LoadYAMLSources();
std::vector<Source> LoadINISources();
void LoadTranslations();

inline std::unordered_map<std::string, bool> other_settings;


namespace LogSettings {
    #ifndef NDEBUG
    inline bool log_trace = true;
    #else
    inline bool log_trace = false;
    #endif
    inline bool log_info = true;
    inline bool log_warning = true;
    inline bool log_error = true;
};