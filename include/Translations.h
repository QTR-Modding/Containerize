#pragma once
#include "MCP.h"
#include "SkyPrompt.h"
#include "Utils.h"

namespace Translations {

    const std::string translations_folder = "Data/SKSE/Plugins/Containerize/translations/";

	const std::unordered_map<std::string, std::string> languageMap = {
        {"ENGLISH", "en"},
        {"GERMAN", "de"},
        {"SPANISH", "es"},
        {"FRENCH", "fr"},
        {"ITALIAN", "it"},
        {"JAPANESE", "ja"},
        {"POLISH", "pl"},
        {"PORTUGUESE", "pt"},
        {"RUSSIAN", "ru"},
        {"CHINESE", "zh"},
        {"KOREAN", "ko"},
        {"TURKISH", "tr"}
    };

    const std::map<std::string, std::map<std::string, std::string*>> requiredTranslations = {
    {
        "OnActivate",
        {
            {"Open", &buttons[0]},
            {"Take", &buttons[1]},
            {"Close", &buttons[3]},
            {"More", &buttons[2]},
            {"Rename", &buttons_more[0]},
            {"Uninstall", &buttons_more[1]},
            {"Back", &buttons_more[2]},
            {"More_Close", &buttons_more[3]}
        }
        },

        {
            "NotifBoxes",
            {
                {"no_src_msgbox", &no_src_msgbox},
                {"po3_err_msgbox", &po3_err_msgbox},
                {"general_err_msgbox", &general_err_msgbox},
                {"init_err_msgbox", &init_err_msgbox},
                {"form_type_err_msgbox", &form_type_err_msgbox},
                {"uninstall_msgbox", &uninstall_msgbox},
                {"uninstall_err_msgbox", &uninstall_err_msgbox},
                {"problem_with_container_msgbox", &problem_with_container_msgbox}
            }
        },

        {
            "MCP",
            {
                {"mod_not_working",&UI::Strings::mod_not_working},
                {"status",&UI::Strings::status},
                {"settings",&UI::Strings::settings},
                {"sources_label",&UI::Strings::sources_label},
                {"inspect",&UI::Strings::inspect},
                {"log",&UI::Strings::log},
                {"uninstall_label",&UI::Strings::uninstall_label},
                {"yaml_error",&UI::Strings::yaml_error},
                {"ini_error",&UI::Strings::ini_error},
                {"duplicate_error",&UI::Strings::duplicate_error},
                {"po3_tweaks",&UI::Strings::po3_tweaks},
                {"installed",&UI::Strings::installed},
                {"not_installed",&UI::Strings::not_installed},
                {"use_or_take",&UI::Strings::use_or_take},
                {"object_manipulation",&UI::Strings::object_manipulation},
                {"collapse_all",&UI::Strings::collapse_all},
                {"expand_all",&UI::Strings::expand_all},
                {"cloud_storage",&UI::Strings::cloud_storage},
                {"capacity",&UI::Strings::capacity},
                {"initial_items",&UI::Strings::initial_items},
                {"no_sources_found",&UI::Strings::no_sources_found},
                {"dynamic_forms",&UI::Strings::dynamic_forms},
                {"no_dynamic_forms",&UI::Strings::no_dynamic_forms},
                {"active",&UI::Strings::active},
                {"protected_status",&UI::Strings::protected_status},
                {"inactive",&UI::Strings::inactive},
                {"log_generate",&UI::Strings::log_generate},
                {"log_trace",&UI::Strings::log_trace},
                {"log_info",&UI::Strings::log_info},
                {"log_warning",&UI::Strings::log_warning},
                {"log_error",&UI::Strings::log_error},
                {"enabled",&UI::Strings::enabled},
                {"disabled",&UI::Strings::disabled},
                {"refresh",&UI::Strings::refresh},
                {"last_generated",&UI::Strings::last_generated},
                {"no_data_found",&UI::Strings::no_data_found},
                {"unknown",&UI::Strings::unknown},
                {"data",&UI::Strings::data},
                {"in_game_hours",&UI::Strings::in_game_hours},
                {"real_form_id",&UI::Strings::real_form_id},
                {"chest_ref_id",&UI::Strings::chest_ref_id},
                {"location_ref_id",&UI::Strings::location_ref_id},
                {"name",&UI::Strings::name},
                {"location_name",&UI::Strings::location_name}
            }
        },
        {"SkyPrompt",
            {
                {"open_bag", &SkyPrompt::Strings::open_bag},
                {"rename_bag", &SkyPrompt::Strings::rename_bag},
                {"weight", &SkyPrompt::Strings::weight},
                {"value", &SkyPrompt::Strings::value},
            }
        }
    };


	std::string GetValidLanguage();

	bool LoadTranslations(const std::string& lang);

};