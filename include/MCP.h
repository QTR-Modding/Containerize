#pragma once
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

    inline ImGuiTableFlags table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

	inline Manager* M;
    void __stdcall RenderStatus();
    void __stdcall RenderSettings();
    void __stdcall RenderSources();
    void __stdcall RenderInspect();
    void __stdcall RenderLog();
    void Register(Manager* manager);

    //inline std::map<RefID, FormID> current_containers;
    inline std::map<FormID,std::pair<std::string,int>> dynamic_forms;
    inline size_t n_sources;
	inline std::vector<ManagerSource> sources;
	inline std::vector<ManagerData> data;

    inline std::map<FormID,bool> collapse_states;
	inline int dft_form_limit = DynamicFormTracker::GetSingleton()->form_limit;

    inline std::string log_path = GetLogPath().string();
    inline std::vector<std::string> logLines;

    inline std::string last_generated;
    void RefreshButton();
    void Refresh();
    void SaveToINI();

    void RenderData();

}