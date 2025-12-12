#include "Settings.h"
#include "Translations.h"
#include "CLibUtilsQTR/FormReader.hpp"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "CLibUtilsQTR/PresetHelpers/PresetHelpersTXT.hpp"
#include "CLibUtilsQTR/PresetHelpers/PresetHelpersYAML.hpp"

namespace {
    Source parseSource_(const YAML::Node& config, const FormID formid, const std::string& editorid) {
        using namespace Settings;

        const auto temp_weight_limit = config["weight_limit"] && !config["weight_limit"].IsNull()
                                           ? config["weight_limit"].as<float>()
                                           : 0.f;

        float cloud_storage = IsCloudStorageEnabled() ? 1.f : 0.f;
        if (config["cloud_storage"] && !config["cloud_storage"].IsNull()) {
            try { cloud_storage = std::clamp(config["cloud_storage"].as<float>(), 0.f, 1.f); } catch (const
                std::exception&) {
                try { cloud_storage = config["cloud_storage"].as<bool>() ? 1.f : 0.f; } catch (const std::exception&) {
                    logger::warn("Cloud storage value is invalid. Using default value.");
                }
            }
        }

        logger::trace("FormEditorID: {}, FormID: {}, WeightLimit: {}, CloudStorage: {}", editorid, formid,
                      temp_weight_limit, cloud_storage);
        Source source(formid, editorid, temp_weight_limit, cloud_storage);

        // add initial items
        if (config["initial_items"] && config["initial_items"].size() > 0) {
            for (const auto& itemNode : config["initial_items"]) {
                auto temp_formeditorid = itemNode["FormEditorID"] && !itemNode["FormEditorID"].IsNull()
                                             ? itemNode["FormEditorID"].as<std::string>()
                                             : "";
                if (!itemNode["count"] || itemNode["count"].IsNull()) {
                    logger::error("Count is null.");
                    continue;
                }
                logger::trace("Count");
                const Count temp_count = itemNode["count"].as<Count>();
                if (temp_count == 0) {
                    logger::error("Count is 0.");
                    continue;
                }

                for (const auto id : PresetHelpers::YAML_Helpers::StringToFormIDs(temp_formeditorid)) {
                    source.AddInitialItem(id, temp_count);
                }
            }
        }

        return source;
    }
}


bool Settings::AnimationsDelayMenuOpen() {
    return other_settings.at(otherstuffKeys.at(6));
}

bool Settings::IsCloudStorageEnabled() {
    return other_settings.at(otherstuffKeys.at(4));
}

bool Settings::IsQuickLootEnabled() {
    return ModCompatibility::Mods::quickloot_installed && other_settings.at(otherstuffKeys.at(8));
}

std::vector<Source> LoadSources() {
    LoadFormGroups();
    std::vector<Source> sources;
    const auto IniSources = LoadINISources();
    logger::trace("IniSources size: {}", IniSources.size());
    //sources.insert(sources.end(), IniSources.begin(), IniSources.end());
    const auto YamlSources = LoadYAMLSources();
    logger::trace("YamlSources size: {}", YamlSources.size());
    //sources.insert(sources.end(), YamlSources.begin(), YamlSources.end());
    std::set<FormID> formids;
    for (const auto& source : YamlSources) {
        if (!source.IsHealthy()) {
            logger::error("Source is not healthy. Skipping. formid {:x} / editorid {} / capacity {}", source.formid,
                          source.editorid, source.capacity);
            Settings::problems_in_YAML_sources |= true;
            continue;
        }
        if (formids.contains(source.formid)) {
            logger::warn("Duplicate formid found. Skipping. formid {:x} / editorid {} / capacity {}", source.formid,
                         source.editorid, source.capacity);
            Settings::duplicate_sources |= true;
            continue;
        }
        formids.insert(source.formid);
        sources.push_back(source);
    }

    for (const auto& source : IniSources) {
        if (!source.IsHealthy()) {
            logger::error("Source is not healthy. Skipping. formid {:x} / editorid {} / capacity {}", source.formid,
                          source.editorid, source.capacity);
            Settings::problems_in_INI_sources |= true;
            continue;
        }
        if (formids.contains(source.formid)) {
            logger::warn("Duplicate formid found. Skipping. formid {:x} / editorid {} / capacity {}", source.formid,
                         source.editorid, source.capacity);
            Settings::duplicate_sources |= true;
            continue;
        }
        formids.insert(source.formid);
        sources.push_back(source);
    }
    return sources;
}

void LoadOtherSettings() {
    using namespace Settings;

    std::unordered_map<std::string, bool> others;

    CSimpleIniA ini;
    CSimpleIniA::TNamesDepend otherkeys;

    ini.SetUnicode();
    ini.LoadFile(path);

    // other stuff section
    for (size_t i = 0; i < otherstuffSize; ++i) {
        if (i == 3) {
            // Skip BatchSell key (index 3). It's always enabled and not configurable.
            continue;
        }
        const auto key = otherstuffKeys[i];
        const bool val = ini.GetBoolValue(InISections[2], key, otherstuffVals[i]);
        other_settings[key] = val;
    }
}

std::vector<Source> parseSources(const YAML::Node& config) {
    std::vector<Source> sources;
    const auto formeditorid = config["FormEditorID"] && !config["FormEditorID"].IsNull()
                                  ? config["FormEditorID"].as<std::string>()
                                  : "";
    const auto candidates = PresetHelpers::YAML_Helpers::StringToFormIDs(formeditorid);
    size_t i = 0;
    for (const auto formid : candidates) {
        if (const auto form = FormReader::GetFormByID(formid)) {
            const auto editorid = clib_util::editorID::get_editorID(form);
            sources.push_back(parseSource_(config, formid, editorid));
            break;
        }
        ++i;
    }
    for (auto k = i + 1; k < candidates.size(); ++k) {
        const auto formid = candidates[k];
        if (const auto form = FormReader::GetFormByID(formid)) {
            const auto editorid = clib_util::editorID::get_editorID(form);
            Source new_source = sources[0];
            new_source.formid = formid;
            new_source.editorid = editorid;
            sources.push_back(new_source);
        }
    }
    return sources;
}

void LoadFormGroups() {
    const auto folder_path = std::format("Data/SKSE/Plugins/{}", mod_name) + "/formGroups";
    PresetHelpers::TXT_Helpers::GatherForms(folder_path);
}

std::vector<Source> LoadYAMLSources() {
    std::vector<Source> sources;
    std::set<FormID> source_formids;
    const auto folder_path = std::format("Data/SKSE/Plugins/{}", mod_name) + "/presets";
    std::filesystem::create_directories(folder_path);
    logger::trace("Custom path: {}", folder_path);
    for (const auto& entry : std::filesystem::directory_iterator(folder_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".yml") {
            const auto filename = entry.path().string();
            YAML::Node config = YAML::LoadFile(filename);

            if (!config["containers"]) {
                continue;
            }

            for (const auto& node : config["containers"]) {
                // we have list of owners at each node or a scalar owner
                try {
                    for (const auto& source : parseSources(node)) {
                        if (!source.IsHealthy()) {
                            logger::error("LoadYAMLSources: File {} has invalid source: {}, {}", filename,
                                          source.formid, source.editorid);
                            continue;
                        }
                        if (!source_formids.contains(source.formid)) {
                            source_formids.insert(source.formid);
                            sources.push_back(source);
                        }
                    }
                } catch (const std::exception& e) {
                    logger::error("Error parsing source: {}", e.what());
                    Settings::problems_in_YAML_sources |= true;
                }
            }
        }
    }
    return sources;
}

std::vector<Source> LoadINISources() {
    using namespace Settings;

    std::vector<Source> sources;

    CSimpleIniA ini;
    CSimpleIniA::TNamesDepend source_names;
    CSimpleIniA::TNamesDepend otherkeys;

    ini.SetUnicode();
    ini.LoadFile(path);

    // Create Sections with defaults if they don't exist
    for (int i = 0; i < 2; ++i) {
        if (!ini.SectionExists(InISections[i])) {
            logger::info("Section {} does not exist. Creating it.", InISections[i]);
            ini.SetValue(InISections[i], nullptr, nullptr);
            ini.SetValue(InISections[i], InIDefaultKeys[i], InIDefaultVals[i], section_comments[i].c_str());
        }
    }

    // Other Stuff section defaults (exclude BatchSell)
    if (!ini.SectionExists(InISections[2])) {
        logger::info("Default values set for section {}", InISections[2]);
        for (size_t i = 0; i < otherstuffSize; ++i) {
            if (i == 3) continue; // skip BatchSell
            ini.SetBoolValue(InISections[2], otherstuffKeys[i], otherstuffVals[i], os_comments[i].c_str());
        }
    } else {
        // Ensure all keys except BatchSell exist; do not overwrite existing values
        ini.GetAllKeys(InISections[2], otherkeys);
        std::unordered_set<std::string> existing;
        for (const auto& k : otherkeys) existing.insert(k.pItem);
        for (size_t i = 0; i < otherstuffSize; ++i) {
            if (i == 3) continue; // skip BatchSell
            const auto key = otherstuffKeys[i];
            if (!existing.contains(key)) {
                ini.SetBoolValue(InISections[2], key, otherstuffVals[i], os_comments[i].c_str());
            }
        }
    }

    // Sections: Containers, Capacities
    ini.GetAllKeys(InISections[0], source_names);
    auto numSources = source_names.size();

    sources.reserve(numSources);

    auto cloud_storage_enabled = IsCloudStorageEnabled();

    for (CSimpleIniA::TNamesDepend::const_iterator it = source_names.begin(); it != source_names.end(); ++it) {
        const char* val1 = ini.GetValue(InISections[0], it->pItem);
        const char* val2 = ini.GetValue(InISections[1], it->pItem);
        if (!val1 || !val2 || !std::strlen(val1) || !std::strlen(val2)) {
            logger::warn("Source {} is missing a value. Skipping.", it->pItem);
            problems_in_INI_sources |= true;
            continue;
        }
        // back to container_id and capacity
        uint32_t id = static_cast<uint32_t>(std::strtoul(val1, nullptr, 16));
        auto id_str = std::string(val1);

        // if both formid is valid hex, use it
        if (FormReader::isValidHexWithLength7or8(val1)) {
            sources.emplace_back(id, "", std::stof(val2), cloud_storage_enabled);
        } else if (!po3installed) {
            logger::error("No formid AND powerofthree's Tweaks is not installed.", val1);
            MsgBoxesNotifs::Windows::Po3ErrMsg();
            return sources;
        } else sources.emplace_back(0, id_str, std::stof(std::string(val2)), cloud_storage_enabled);

        logger::trace("Source {} has a value of {}", it->pItem, val1);
        ini.SetValue(InISections[0], it->pItem, val1);
        ini.SetValue(InISections[1], it->pItem, val2);
    }

    ini.SaveFile(path);

    return sources;
}

void LoadTranslations() {
    logger::info("Loading translations");
    const auto lang = Translations::GetValidLanguage();
    logger::info("Game language: {}", lang);
    if (Translations::LoadTranslations(lang)) {
        logger::info("Translations loaded.");
    } else {
        logger::warn("Failed to load translations.");
    }
}