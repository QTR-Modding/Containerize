#include "MCP.h"

void HelpMarker(const char* desc)
{
	ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void __stdcall UI::RenderStatus()
{
    constexpr auto color_operational = ImVec4(0, 1, 0, 1);
    constexpr auto color_not_operational = ImVec4(1, 0, 0, 1);

    if (!M) {
		ImGui::TextColored(color_not_operational, "Mod is not working! Check log for more info.");
        return;
	}
    ImGui::Text("Status: ");
	ImGui::SameLine();
    ImGui::TextColored(color_operational, std::format("Sources ({})",n_sources).c_str());
	ImGui::SameLine();
	if (ImGui::Button("Uninstall")) M->Uninstall();

	if (Settings::problems_in_YAML_sources) {
		ImGui::TextColored(color_not_operational, "Problems in YAML files. Check log for more info.");
	}
	if (Settings::problems_in_INI_sources) {
		ImGui::TextColored(color_not_operational, "Problems in INI file. Check log for more info.");
	}
	if (Settings::duplicate_sources) {
		ImGui::TextColored(color_not_operational, "Duplicate sources from INI and YAML files found. Check log for more info.");
	}

    ImGui::Text("");
    ImGui::Text("po3's Tweaks: ");
	ImGui::SameLine();
	ImGui::TextColored(Settings::po3installed ? color_operational : color_not_operational, Settings::po3installed ? "Installed" : "Not Installed");

	ImGui::Text("Use or Take: ");
	ImGui::SameLine();
	ImGui::TextColored(po3_use_or_take ? color_operational : color_not_operational, po3_use_or_take ? "Installed" : "Not Installed");

	ImGui::Text("Object Manipulation Overhaul: ");
	ImGui::SameLine();
	ImGui::TextColored(obj_manipu_installed ? color_operational : color_not_operational, obj_manipu_installed ? "Installed" : "Not Installed");



}

void __stdcall UI::RenderSettings()
{
	bool settings_changed = false;
    for (auto& [setting_name, setting] : other_settings) {
		settings_changed |= ImGui::Checkbox((setting_name+":").c_str(), &setting);
		ImGui::SameLine();
        const char* value = setting ? "Enabled" : "Disabled";
        const auto color = setting ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1);
		ImGui::TextColored(color, value);
		ImGui::SameLine();
		HelpMarker(Settings::os_comments[std::distance(Settings::otherstuffKeys.begin(), std::ranges::find(Settings::otherstuffKeys, setting_name))].c_str());
    }
	if (settings_changed) SaveToINI();
}

void __stdcall UI::RenderSources()
{

	RefreshButton();

	ImGui::Text(std::format("Sources ({})",n_sources).c_str());
	if (sources.empty()) {
		ImGui::Text("No sources found.");
		return;
	}

	// collapse all and expand all buttons
	if (ImGui::Button("Collapse All")) {
		for (auto& state : collapse_states | std::views::values) {
			state = false;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Expand All")) {
		for (auto& state : collapse_states | std::views::values) {
			state = true;
		}
	}

	// collapsable: FormID, EditorID, Cloud Storage Ratio, Capacity, Initial Items
	for (const auto& source : sources) {
		if (!collapse_states[source.formid]) ImGui::SetNextItemOpen(false);
        else ImGui::SetNextItemOpen(true);
		if (ImGui::CollapsingHeader(std::format("{:08X} - {}", source.formid, source.editorid).c_str())) {
			ImGui::Text("Cloud Storage: %.2f%%", source.cloud_storage_ratio * 100);
			ImGui::Text("Capacity: %.2f", source.capacity);
			ImGui::Text("Initial Items");
			if (ImGui::BeginTable("table_initial_items", 2, table_flags)) {
				for (const auto& [formid, item] : source.initial_items) {
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text(std::format("{:08X}", formid).c_str());
					ImGui::TableNextColumn();
					ImGui::Text(std::format("{} x{}", item.first, item.second).c_str());
				}
				ImGui::EndTable();
			}
			collapse_states[source.formid] = true;
		}
		else collapse_states[source.formid] = false;
	}

}

void __stdcall UI::RenderInspect()
{
	RefreshButton();

	ImGui::Text(std::format("Dynamic Forms ({}/{})", dynamic_forms.size(),dft_form_limit).c_str());
	if (dynamic_forms.empty()) {
		ImGui::Text("No dynamic forms found.");
		return;
	}
	// dynamic forms table: FormID, Name, Status
	if (ImGui::BeginTable("table_dynamic_forms", 3, table_flags)) {
		for (const auto& [formid, form] : dynamic_forms) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text(std::format("{:08X}", formid).c_str());
			ImGui::TableNextColumn();
			ImGui::Text(form.first.c_str());
			ImGui::TableNextColumn();
			const auto color = form.second == 2 ? ImVec4(0, 1, 0, 1) : form.second == 1 ? ImVec4(1, 1, 0, 1) : ImVec4(1, 0, 0, 1);
			ImGui::TextColored(color, form.second == 2 ? "Active" : form.second == 1 ? "Protected" : "Inactive");
		}
		ImGui::EndTable();
	}

	RenderData();

}

void __stdcall UI::RenderLog()
{
#ifndef NDEBUG
    ImGui::Checkbox("Trace", &LogSettings::log_trace);
#endif
    ImGui::SameLine();
    ImGui::Checkbox("Info", &LogSettings::log_info);
    ImGui::SameLine();
    ImGui::Checkbox("Warning", &LogSettings::log_warning);
    ImGui::SameLine();
    ImGui::Checkbox("Error", &LogSettings::log_error);

    // if "Generate Log" button is pressed, read the log file
    if (ImGui::Button("Generate Log")) logLines = ReadLogFile();

    // Display each line in a new ImGui::Text() element
    for (const auto& line : logLines) {
        if (!LogSettings::log_trace && line.find("trace") != std::string::npos) continue;
        if (!LogSettings::log_info && line.find("info") != std::string::npos) continue;
        if (!LogSettings::log_warning && line.find("warning") != std::string::npos) continue;
        if (!LogSettings::log_error && line.find("error") != std::string::npos) continue;
        ImGui::Text(line.c_str());
    }
}

void UI::Register(Manager* manager)
{
    if (!SKSEMenuFramework::IsInstalled()) {
        return;
    }
    SKSEMenuFramework::SetSection(mod_name);
    SKSEMenuFramework::AddSectionItem("Status", RenderStatus);
    SKSEMenuFramework::AddSectionItem("Settings", RenderSettings);
	SKSEMenuFramework::AddSectionItem("Sources", RenderSources);
    SKSEMenuFramework::AddSectionItem("Inspect", RenderInspect);
    SKSEMenuFramework::AddSectionItem("Log", RenderLog);
    M = manager;
	n_sources = M->GetSources().size();
}

void UI::RefreshButton()
{
    FontAwesome::PushSolid();

    if (ImGui::Button((FontAwesome::UnicodeToUtf8(0xf021) + " Refresh").c_str()) || last_generated.empty()) {
		Refresh();
    }
    FontAwesome::Pop();

    ImGui::SameLine();
    ImGui::Text(("Last Generated: " + last_generated).c_str());
}

void UI::Refresh()
{

    last_generated = std::format("{} (in-game hours)", RE::Calendar::GetSingleton()->GetHoursPassed());
	dynamic_forms.clear();
    for (const auto DFT = DynamicFormTracker::GetSingleton(); const auto& df : DFT->GetDynamicForms()) {
		if (const auto form = RE::TESForm::LookupByID(df); form) {
			auto status = DFT->IsActive(df) ? 2 : DFT->IsProtected(df) ? 1 : 0;
			dynamic_forms[df] = { form->GetName(), status };
		}
    }

	collapse_states.clear();
	data.clear();
	const auto& sources_temp= M->GetSources();
	n_sources = sources_temp.size();
	for (const auto& source : sources_temp) {
		ManagerSource mcp_source;
		mcp_source.formid = source.formid;
		mcp_source.editorid = source.editorid;
		mcp_source.cloud_storage_ratio = 1 - source.weight_ratio;
		mcp_source.capacity = source.capacity;
		for (const auto& [formid, count] : source.initial_items) {
			const auto form = RE::TESForm::LookupByID(formid);
			auto temp_name = std::string(form->GetName());
			temp_name = temp_name.empty() ? clib_util::editorID::get_editorID(form) : temp_name;
			mcp_source.initial_items[formid] = { temp_name, count };
		}
		sources.push_back(mcp_source);
		collapse_states[source.formid] = false;

		// data
		for (const auto& [chest_ref, location] : source.data) {
			ManagerData mcp_data;
			mcp_data.real_formid = source.formid;
			mcp_data.chest_ref = chest_ref;
			mcp_data.location = location;
			const auto real_item = RE::TESForm::LookupByID(source.formid);
			mcp_data.name = real_item ? real_item->GetName() : "Unknown";
			const auto loc = RE::TESForm::LookupByID(location);
			mcp_data.location_name = loc ? loc->GetName() : "Unknown";
			data.push_back(mcp_data);
		}
    }



}

void UI::SaveToINI()
{
    using namespace Settings;

    CSimpleIniA ini;

    ini.SetUnicode();
    ini.LoadFile(path);
    // other stuff section
	for (const auto& [setting_name, setting] : other_settings) {
		ini.SetBoolValue(InISections[2], setting_name.c_str(), setting);
	}

	ini.SaveFile(path);

}

void UI::RenderData()
{
	ImGui::Text(std::format("Data ({})", data.size()).c_str());
	if (data.empty()) {
		ImGui::Text("No data found.");
		return;
	}
	// data table: Real FormID, Chest RefID, Location RefID, Name, Location Name
	// make sure to write these at the top of each column
	if (ImGui::BeginTable("table_data", 5, table_flags)) {
		ImGui::TableSetupColumn("Real FormID");
		ImGui::TableSetupColumn("Chest RefID");
		ImGui::TableSetupColumn("Location RefID");
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Location Name");
		ImGui::TableHeadersRow();
		for (const auto& data_ : data) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text(std::format("{:x}", data_.real_formid).c_str());
			ImGui::TableNextColumn();
			ImGui::Text(std::format("{:x}", data_.chest_ref).c_str());
			ImGui::TableNextColumn();
			ImGui::Text(std::format("{:x}", data_.location).c_str());
			ImGui::TableNextColumn();
			ImGui::Text(data_.name.c_str());
			ImGui::TableNextColumn();
			ImGui::Text(data_.location_name.c_str());
		}
		ImGui::EndTable();
	}
}

