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
        ImGui::TextColored(color_not_operational, Strings::mod_not_working.c_str());
        return;
    }

    ImGui::Text((Strings::status + ": ").c_str());
    ImGui::SameLine();
    ImGui::TextColored(color_operational, std::format("{} ({})", Strings::sources_label, n_sources).c_str());
    ImGui::SameLine();
    if (ImGui::Button(Strings::uninstall_label.c_str())) M->Uninstall();

    if (Settings::problems_in_YAML_sources) {
        ImGui::TextColored(color_not_operational, Strings::yaml_error.c_str());
    }
    if (Settings::problems_in_INI_sources) {
        ImGui::TextColored(color_not_operational, Strings::ini_error.c_str());
    }
    if (Settings::duplicate_sources) {
        ImGui::TextColored(color_not_operational, Strings::duplicate_error.c_str());
    }

    ImGui::Text("");
    ImGui::Text((Strings::po3_tweaks + ": ").c_str());
    ImGui::SameLine();
    ImGui::TextColored(Settings::po3installed ? color_operational : color_not_operational, Settings::po3installed ? Strings::installed.c_str() : Strings::not_installed.c_str());

    ImGui::Text((Strings::use_or_take + ": ").c_str());
    ImGui::SameLine();
    ImGui::TextColored(po3_use_or_take ? color_operational : color_not_operational, po3_use_or_take ? Strings::installed.c_str() : Strings::not_installed.c_str());

    ImGui::Text((Strings::object_manipulation + ": ").c_str());
    ImGui::SameLine();
    ImGui::TextColored(obj_manipu_installed ? color_operational : color_not_operational, obj_manipu_installed ? Strings::installed.c_str() : Strings::not_installed.c_str());
}

void __stdcall UI::RenderSettings()
{
	bool settings_changed = false;
    for (auto& [setting_name, setting] : other_settings) {
		settings_changed |= ImGui::Checkbox((setting_name+":").c_str(), &setting);
		ImGui::SameLine();
        const char* value = setting ? Strings::enabled.c_str() : Strings::disabled.c_str();
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

    ImGui::Text(std::format("{} ({})", Strings::sources_label, n_sources).c_str());
    if (sources.empty()) {
        ImGui::Text(Strings::no_sources_found.c_str());
        return;
    }

    // collapse all and expand all buttons
    if (ImGui::Button(Strings::collapse_all.c_str())) {
        for (auto& state : collapse_states | std::views::values) {
            state = false;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(Strings::expand_all.c_str())) {
        for (auto& state : collapse_states | std::views::values) {
            state = true;
        }
    }

    // collapsable: FormID, EditorID, Cloud Storage Ratio, Capacity, Initial Items
    for (const auto& source : sources) {
        if (!collapse_states[source.formid]) ImGui::SetNextItemOpen(false);
        else ImGui::SetNextItemOpen(true);
        if (ImGui::CollapsingHeader(std::format("{:08X} - {}", source.formid, source.editorid).c_str())) {
            ImGui::Text(std::format("{}: %.2f%%", Strings::cloud_storage, source.cloud_storage_ratio * 100).c_str());
            ImGui::Text(std::format("{}: %.2f", Strings::capacity, source.capacity).c_str());
            ImGui::Text(Strings::initial_items.c_str());
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

	ImGui::Text(std::format("{} ({}/{})", Strings::dynamic_forms, dynamic_forms.size(), dft_form_limit).c_str());
	if (dynamic_forms.empty()) {
		ImGui::Text(Strings::no_dynamic_forms.c_str());
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
			ImGui::TextColored(color, form.second == 2 ? Strings::active.c_str() : form.second == 1 ? Strings::protected_status.c_str() : Strings::inactive.c_str());
		}
		ImGui::EndTable();
	}

	RenderData();
}

void __stdcall UI::RenderLog()
{
#ifndef NDEBUG
    ImGui::Checkbox(Strings::log_trace.c_str(), &LogSettings::log_trace);
#endif
    ImGui::SameLine();
    ImGui::Checkbox(Strings::log_info.c_str(), &LogSettings::log_info);
    ImGui::SameLine();
    ImGui::Checkbox(Strings::log_warning.c_str(), &LogSettings::log_warning);
    ImGui::SameLine();
    ImGui::Checkbox(Strings::log_error.c_str(), &LogSettings::log_error);

    // if "Generate Log" button is pressed, read the log file
    if (ImGui::Button(Strings::log_generate.c_str())) logLines = ReadLogFile();

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
    SKSEMenuFramework::AddSectionItem(Strings::status, RenderStatus);
    SKSEMenuFramework::AddSectionItem(Strings::settings, RenderSettings);
    SKSEMenuFramework::AddSectionItem(Strings::sources_label, RenderSources);
    SKSEMenuFramework::AddSectionItem(Strings::inspect, RenderInspect);
    SKSEMenuFramework::AddSectionItem(Strings::log, RenderLog);
    M = manager;
    n_sources = M->GetSources().size();
}

void UI::RefreshButton()
{
    FontAwesome::PushSolid();

    if (ImGui::Button((FontAwesome::UnicodeToUtf8(0xf021) + " " + Strings::refresh).c_str()) || last_generated.empty()) {
		Refresh();
    }
    FontAwesome::Pop();

    ImGui::SameLine();
    ImGui::Text((Strings::last_generated + last_generated).c_str());
}

void UI::Refresh()
{

    last_generated = std::format("{} ({})",RE::Calendar::GetSingleton()->GetHoursPassed(),Strings::in_game_hours);
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
			mcp_data.name = real_item ? real_item->GetName() : Strings::unknown;
			const auto loc = RE::TESForm::LookupByID(location);
			mcp_data.location_name = loc ? loc->GetName() : Strings::unknown;
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
    ImGui::Text(std::format("{} ({})", Strings::data, data.size()).c_str());
    if (data.empty()) {
        ImGui::Text(Strings::no_data_found.c_str());
        return;
    }
    
    if (ImGui::BeginTable("table_data", 5, table_flags)) {
        ImGui::TableSetupColumn(Strings::real_form_id.c_str());
        ImGui::TableSetupColumn(Strings::chest_ref_id.c_str());
        ImGui::TableSetupColumn(Strings::location_ref_id.c_str());
        ImGui::TableSetupColumn(Strings::name.c_str());
        ImGui::TableSetupColumn(Strings::location_name.c_str());
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

