#include "MCP.h"

#include "Manager.h"
#include "Settings.h"

void HelpMarker(const char* desc) {
    ImGuiMCP::TextDisabled("(?)");
    if (ImGuiMCP::BeginItemTooltip()) {
        ImGuiMCP::PushTextWrapPos(ImGuiMCP::GetFontSize() * 35.0f);
        ImGuiMCP::TextUnformatted(desc);
        ImGuiMCP::PopTextWrapPos();
        ImGuiMCP::EndTooltip();
    }
}

void __stdcall UI::RenderStatus() {
    constexpr auto color_operational = ImVec4(0, 1, 0, 1);
    constexpr auto color_not_operational = ImVec4(1, 0, 0, 1);

    Text((Strings::status + ": ").c_str());
    SameLine();
    TextColored(color_operational, std::format("{} ({})", Strings::sources_label, n_sources).c_str());
    SameLine();
    if (Button(Strings::uninstall_label.c_str())) Manager::GetSingleton()->Uninstall();

    if (Settings::problems_in_YAML_sources) {
        TextColored(color_not_operational, Strings::yaml_error.c_str());
    }
    if (Settings::problems_in_INI_sources) {
        TextColored(color_not_operational, Strings::ini_error.c_str());
    }
    if (Settings::duplicate_sources) {
        TextColored(color_not_operational, Strings::duplicate_error.c_str());
    }

    Text("");
    Text((Strings::po3_tweaks + ": ").c_str());
    SameLine();
    TextColored(Settings::po3installed ? color_operational : color_not_operational,
                Settings::po3installed ? Strings::installed.c_str() : Strings::not_installed.c_str());

    Text((Strings::use_or_take + ": ").c_str());
    SameLine();

    using namespace ModCompatibility::Mods;
    TextColored(po3_use_or_take ? color_operational : color_not_operational,
                po3_use_or_take ? Strings::installed.c_str() : Strings::not_installed.c_str());

    Text((Strings::object_manipulation + ": ").c_str());
    SameLine();
    TextColored(obj_manipu_installed ? color_operational : color_not_operational,
                obj_manipu_installed ? Strings::installed.c_str() : Strings::not_installed.c_str());
}

void __stdcall UI::RenderSettings() {
    bool settings_changed = false;
    for (auto& [setting_name, setting] : Settings::other_settings) {
        // Skip BatchSell (always enabled and not user-configurable)
        if (setting_name == Settings::otherstuffKeys[3]) {
            continue;
        }
        settings_changed |= Checkbox((setting_name + ":").c_str(), &setting);
        SameLine();
        const char* value = setting ? Strings::enabled.c_str() : Strings::disabled.c_str();
        const auto color = setting ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1);
        TextColored(color, value);
        SameLine();
        HelpMarker(Settings::os_comments[std::distance(Settings::otherstuffKeys.begin(),
                                                       std::ranges::find(Settings::otherstuffKeys, setting_name))].
            c_str());
    }
    if (settings_changed) SaveToINI();
}

void __stdcall UI::RenderSources() {
    RefreshButton();

    Text(std::format("{} ({})", Strings::sources_label, n_sources).c_str());
    if (sources.empty()) {
        Text(Strings::no_sources_found.c_str());
        return;
    }

    // collapse all and expand all buttons
    if (Button(Strings::collapse_all.c_str())) {
        for (auto& state : collapse_states | std::views::values) {
            state = false;
        }
    }
    SameLine();
    if (Button(Strings::expand_all.c_str())) {
        for (auto& state : collapse_states | std::views::values) {
            state = true;
        }
    }

    // collapsable: FormID, EditorID, Cloud Storage Ratio, Capacity, Initial Items
    for (const auto& source : sources) {
        if (!collapse_states[source.formid]) SetNextItemOpen(false);
        else SetNextItemOpen(true);
        if (CollapsingHeader(std::format("{:08X} - {}", source.formid, source.editorid).c_str())) {
            Text("%s: %.2f%%", Strings::cloud_storage.c_str(), source.cloud_storage_ratio * 100);
            Text(std::format("{}: %.2f", Strings::capacity, source.capacity).c_str());
            Text(Strings::initial_items.c_str());
            if (BeginTable("table_initial_items", 2, table_flags)) {
                for (const auto& [formid, item] : source.initial_items) {
                    TableNextRow();
                    TableNextColumn();
                    Text(std::format("{:08X}", formid).c_str());
                    TableNextColumn();
                    Text(std::format("{} x{}", item.first, item.second).c_str());
                }
                EndTable();
            }
            collapse_states[source.formid] = true;
        } else collapse_states[source.formid] = false;
    }
}

void __stdcall UI::RenderInspect() {
    RefreshButton();

    Text(std::format("{} ({}/{})", Strings::dynamic_forms, dynamic_forms.size(), dft_form_limit).c_str());
    if (dynamic_forms.empty()) {
        Text(Strings::no_dynamic_forms.c_str());
        return;
    }
    // dynamic forms table: FormID, Name, Status
    if (BeginTable("table_dynamic_forms", 3, table_flags)) {
        for (const auto& [formid, form] : dynamic_forms) {
            TableNextRow();
            TableNextColumn();
            Text(std::format("{:08X}", formid).c_str());
            TableNextColumn();
            Text(form.first.c_str());
            TableNextColumn();
            const auto color = form.second == 2
                                   ? ImVec4(0, 1, 0, 1)
                                   : form.second == 1
                                   ? ImVec4(1, 1, 0, 1)
                                   : ImVec4(1, 0, 0, 1);
            TextColored(color, form.second == 2
                                   ? Strings::active.c_str()
                                   : form.second == 1
                                   ? Strings::protected_status.c_str()
                                   : Strings::inactive.c_str());
        }
        EndTable();
    }

    RenderData();
}

void __stdcall UI::RenderLog() {
    #ifndef NDEBUG
    Checkbox(Strings::log_trace.c_str(), &LogSettings::log_trace);
    #endif
    SameLine();
    Checkbox(Strings::log_info.c_str(), &LogSettings::log_info);
    SameLine();
    Checkbox(Strings::log_warning.c_str(), &LogSettings::log_warning);
    SameLine();
    Checkbox(Strings::log_error.c_str(), &LogSettings::log_error);

    // if "Generate Log" button is pressed, read the log file
    if (Button(Strings::log_generate.c_str())) logLines = ReadLogFile();

    // Display each line in a new ImGui::Text() element
    for (const auto& line : logLines) {
        if (!LogSettings::log_trace && line.find("trace") != std::string::npos) continue;
        if (!LogSettings::log_info && line.find("info") != std::string::npos) continue;
        if (!LogSettings::log_warning && line.find("warning") != std::string::npos) continue;
        if (!LogSettings::log_error && line.find("error") != std::string::npos) continue;
        Text(line.c_str());
    }
}

void UI::Register() {
    if (!SKSEMenuFramework::IsInstalled()) {
        return;
    }
    SKSEMenuFramework::SetSection(mod_name);
    SKSEMenuFramework::AddSectionItem(Strings::status, RenderStatus);
    SKSEMenuFramework::AddSectionItem(Strings::settings, RenderSettings);
    SKSEMenuFramework::AddSectionItem(Strings::sources_label, RenderSources);
    SKSEMenuFramework::AddSectionItem(Strings::inspect, RenderInspect);
    SKSEMenuFramework::AddSectionItem(Strings::log, RenderLog);
    n_sources = Manager::GetSingleton()->GetSources().size();
    logger::info("MCP registered.");
}

void UI::RefreshButton() {
    FontAwesome::PushSolid();

    if (Button((FontAwesome::UnicodeToUtf8(0xf021) + " " + Strings::refresh).c_str()) || last_generated.empty()) {
        Refresh();
    }
    FontAwesome::Pop();

    SameLine();
    Text((Strings::last_generated + last_generated).c_str());
}

void UI::Refresh() {
    last_generated = std::format("{} ({})", RE::Calendar::GetSingleton()->GetHoursPassed(), Strings::in_game_hours);
    dynamic_forms.clear();
    for (const auto DFT = DynamicFormTracker::GetSingleton(); const auto& df : DFT->GetDynamicForms()) {
        if (const auto form = RE::TESForm::LookupByID(df); form) {
            auto status = DFT->IsActive(df) ? 2 : DFT->IsProtected(df) ? 1 : 0;
            dynamic_forms[df] = {form->GetName(), status};
        }
    }

    collapse_states.clear();
    data.clear();
    const auto& sources_temp = Manager::GetSingleton()->GetSources();
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
            mcp_source.initial_items[formid] = {temp_name, count};
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

void UI::SaveToINI() {
    using namespace Settings;

    CSimpleIniA ini;

    ini.SetUnicode();
    ini.LoadFile(path);
    // other stuff section
    for (const auto& [setting_name, setting] : other_settings) {
        if (setting_name == otherstuffKeys[3]) continue; // skip BatchSell
        ini.SetBoolValue(InISections[2], setting_name.c_str(), setting);
    }

    ini.SaveFile(path);
}

void UI::RenderData() {
    Text(std::format("{} ({})", Strings::data, data.size()).c_str());
    if (data.empty()) {
        Text(Strings::no_data_found.c_str());
        return;
    }

    if (BeginTable("table_data", 5, table_flags)) {
        TableSetupColumn(Strings::real_form_id.c_str());
        TableSetupColumn(Strings::chest_ref_id.c_str());
        TableSetupColumn(Strings::location_ref_id.c_str());
        TableSetupColumn(Strings::name.c_str());
        TableSetupColumn(Strings::location_name.c_str());
        TableHeadersRow();

        for (const auto& data_ : data) {
            TableNextRow();
            TableNextColumn();
            Text(std::format("{:x}", data_.real_formid).c_str());
            TableNextColumn();
            Text(std::format("{:x}", data_.chest_ref).c_str());
            TableNextColumn();
            Text(std::format("{:x}", data_.location).c_str());
            TableNextColumn();
            Text(data_.name.c_str());
            TableNextColumn();
            Text(data_.location_name.c_str());
        }
        EndTable();
    }
}