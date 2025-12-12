#pragma once
#include "DynamicFormTracker.h"
#include "Events.h"
#include "SKSEMCP/SKSEMenuFramework.hpp"


static void HelpMarker(const char* desc);

struct ManagerSource {
	std::uint32_t formid;
	std::string editorid;
	float cloud_storage_ratio;
	float capacity;
	std::map<FormID, std::pair<std::string,Count>> initial_items;
};

struct ManagerData {
    FormID real_formid;
	RefID chest_ref;
    RefID location;
	std::string name;
	std::string location_name;
};

namespace UI {
    using namespace ImGuiMCP;

    inline ImGuiTableFlags table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

    void __stdcall RenderStatus();
    void __stdcall RenderSettings();
    void __stdcall RenderSources();
    void __stdcall RenderInspect();
    void __stdcall RenderLog();
    void Register();

    //inline std::map<RefID, FormID> current_containers;
    inline std::map<FormID,std::pair<std::string,int>> dynamic_forms;
    inline size_t n_sources;
	inline std::vector<ManagerSource> sources;
	inline std::vector<ManagerData> data;

    inline std::map<FormID,bool> collapse_states;
	inline unsigned int dft_form_limit = DynamicFormTracker::GetSingleton()->form_limit;

    inline std::string log_path = GetLogPath().string();
    inline std::vector<std::string> logLines;

    inline std::string last_generated;
    void RefreshButton();
    void Refresh();
    void SaveToINI();

    void RenderData();

    namespace Strings {
        inline std::string mod_not_working = "Mod is not working! Check log for more info.";
        inline std::string status = "Status";
        inline std::string settings = "Settings";
        inline std::string sources_label = "Sources";
        inline std::string inspect = "Inspect";
        inline std::string log = "Log";
        inline std::string uninstall_label = "Uninstall";
        inline std::string yaml_error = "Problems in YAML files. Check log for more info.";
        inline std::string ini_error = "Problems in INI file. Check log for more info.";
        inline std::string duplicate_error = "Duplicate sources from INI and YAML files found. Check log for more info.";
        inline std::string po3_tweaks = "po3's Tweaks";
        inline std::string installed = "Installed";
        inline std::string not_installed = "Not Installed";
        inline std::string use_or_take = "Use or Take";
        inline std::string object_manipulation = "Object Manipulation Overhaul";
        inline std::string collapse_all = "Collapse All";
        inline std::string expand_all = "Expand All";
        inline std::string cloud_storage = "Cloud Storage";
        inline std::string capacity = "Capacity";
        inline std::string initial_items = "Initial Items";
        inline std::string no_sources_found = "No sources found.";
        inline std::string dynamic_forms = "Dynamic Forms";
        inline std::string no_dynamic_forms = "No dynamic form found.";
        inline std::string active = "Active";
        inline std::string protected_status = "Protected";
        inline std::string inactive = "Inactive";
        inline std::string log_generate = "Generate Log";
        inline std::string log_trace = "Trace";
        inline std::string log_info = "Info";
        inline std::string log_warning = "Warning";
        inline std::string log_error = "Error";

        inline std::string enabled = "Enabled";
        inline std::string disabled = "Disabled";
        inline std::string refresh = "Refresh";
        inline std::string last_generated = "Last Generated: ";
        inline std::string no_data_found = "No data found."; 
        inline std::string unknown = "Unknown";
        inline std::string data = "Data";
        inline std::string in_game_hours = "in-game hours";
        
        // **Table Headers**
        inline std::string real_form_id = "Real FormID";
        inline std::string chest_ref_id = "Chest RefID";
        inline std::string location_ref_id = "Location RefID";
        inline std::string name = "Name";
        inline std::string location_name = "Location Name";

    };


}