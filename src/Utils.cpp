#include <Utils.h>
#include "Translations.h"
#include "CLibUtilsQTR/FormReader.hpp"


bool GetDllVersion(const std::wstring& dllPath, DWORD& major, DWORD& minor, DWORD& build, DWORD& revision) {
    DWORD handle = 0;
    const DWORD versionInfoSize = GetFileVersionInfoSize(dllPath.c_str(), &handle);
    if (versionInfoSize == 0) {
        //logger::error("Failed to get version info size for {}", dllPath);
        return false;
    }

    // Allocate a buffer for version info data
    std::vector<char> versionInfo(versionInfoSize);
    if (!GetFileVersionInfo(dllPath.c_str(), handle, versionInfoSize, versionInfo.data())) {
        //logger::error("Failed to get version info for {}", dllPath);
        return false;
    }

    // Query the version value
    VS_FIXEDFILEINFO* fileInfo = nullptr;
    UINT fileInfoSize = 0;
    if (!VerQueryValue(versionInfo.data(), L"\\", reinterpret_cast<LPVOID*>(&fileInfo), &fileInfoSize)) {
        //logger::error("Failed to query version value for {}", dllPath);
        return false;
    }

    // Extract version numbers
    major = (fileInfo->dwFileVersionMS >> 16) & 0xFFFF;
    minor = (fileInfo->dwFileVersionMS) & 0xFFFF;
    build = (fileInfo->dwFileVersionLS >> 16) & 0xFFFF;
    revision = (fileInfo->dwFileVersionLS) & 0xFFFF;

    return true;
}

std::wstring s2ws(const std::string& str) {
    const int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), nullptr, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), wstrTo.data(), size_needed);
    return wstrTo;
}

bool ModCompatibility::Mods::IsPo3Installed() {
    using namespace ModCompatibility::Mods;
    if (!std::filesystem::exists(po3path)) {
        return false;
    }
    // check version of the dll. its major version should be larger equal to 1 and minor version should be larger equal to 12.
    DWORD major, minor, build, revision;
    if (!GetDllVersion(s2ws(std::string(po3path)), major, minor, build, revision)) {
        logger::error("Failed to get version info for {}", po3path);
        return false;
    }
    if (major < 1 || (major == 1 && minor < 9)) {
        logger::error("Po3's Tweaks version is lower than the required version. Major: {}, Minor: {}", major, minor);
        return false;
    }
    return true;
}

void SetupLog() {
    const auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
    auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    const auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));
    spdlog::set_default_logger(std::move(loggerPtr));
    #ifndef NDEBUG
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::trace);
    #else
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
    #endif
    logger::info("Name of the plugin is {}.", pluginName);
    logger::info("Version of the plugin is {}.", SKSE::PluginDeclaration::GetSingleton()->GetVersion());
}

std::filesystem::path GetLogPath() {
    const auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
    auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
    return logFilePath;
}

std::vector<std::string> ReadLogFile() {
    std::vector<std::string> logLines;

    // Open the log file
    std::ifstream file(GetLogPath().c_str());
    if (!file.is_open()) {
        // Handle error
        return logLines;
    }

    // Read and store each line from the file
    std::string line;
    while (std::getline(file, line)) {
        logLines.push_back(line);
    }

    file.close();

    return logLines;
}

std::string DecodeTypeCode(const std::uint32_t typeCode) {
    char buf[4];
    buf[3] = static_cast<char>(typeCode);
    buf[2] = static_cast<char>(typeCode >> 8);
    buf[1] = static_cast<char>(typeCode >> 16);
    buf[0] = static_cast<char>(typeCode >> 24);
    return std::string(buf, buf + 4);
}


std::string GetGameLanguage() {
    if (const RE::Setting* languageSetting = RE::GetINISetting("sLanguage:General")) {
        std::string lang = languageSetting->GetString();
        std::ranges::transform(lang, lang.begin(), toupper);
        if (const auto it = Translations::languageMap.find(lang); it != Translations::languageMap.end()) {
            return it->first;
        }
        logger::warn("Detected language is not supported: {}", lang);
    } else {
        logger::error("Failed to get sLanguage setting.");
    }
    return "ENGLISH";
}

int32_t FunctionsSkyrim::GetEnchantmentCostOverride(const RE::EnchantmentItem* enchantment) {
    int32_t extra_costs = 0;
    auto temp_costoverride = enchantment->data.costOverride;
    if (temp_costoverride < 0) temp_costoverride = static_cast<int32_t>(enchantment->CalculateTotalGoldValue());
    if (temp_costoverride < 0)
        temp_costoverride =
            static_cast<int32_t>(enchantment->CalculateTotalGoldValue(RE::PlayerCharacter::GetSingleton()));
    if (temp_costoverride > 0) {
        extra_costs += temp_costoverride;
    }

    return extra_costs;
}

int32_t FunctionsSkyrim::GetItemValue(RE::TESBoundObject* item, const RE::ExtraDataList* a_xlist) {
    int32_t value = 0;
    if (const auto val_form = item->As<RE::TESValueForm>()) {
        value += val_form->value;
    }
    if (const auto ench_form = item->As<RE::TESEnchantableForm>()) {
        if (const auto ench = ench_form->formEnchanting) {
            value += GetEnchantmentCostOverride(ench);
        }
    }
    value += xData::GetXDataCostOverride(a_xlist);
    return value;
}

bool xData::UpdateExtras(RE::TESObjectREFR* copy_from, RE::TESObjectREFR* copy_to) {
    if (!copy_from || !copy_to) {
        logger::error("copy_from or copy_to is null");
        return false;
    }
    auto* copy_from_extralist = &copy_from->extraList;
    auto* copy_to_extralist = &copy_to->extraList;
    return UpdateExtras(copy_from_extralist, copy_to_extralist);
}


bool xData::UpdateExtras(RE::ExtraDataList* copy_from, RE::ExtraDataList* copy_to) {
    if (!copy_from || !copy_to) return false;
    // Enchantment
    if (copy_from->HasType(RE::ExtraDataType::kEnchantment)) {
        logger::trace("Enchantment found");
        if (const auto enchantment = copy_from->GetByType<RE::ExtraEnchantment>()) {
            if (RE::ExtraEnchantment* enchantment_fake = RE::BSExtraData::Create<RE::ExtraEnchantment>()) {
                // log the associated actor value
                logger::trace("Associated actor value: {}", enchantment->enchantment->GetAssociatedSkill());
                Copy::CopyEnchantment(enchantment, enchantment_fake);
                copy_to->Add(enchantment_fake);
            } else return false;
        } else return false;
    }
    // Health
    if (copy_from->HasType(RE::ExtraDataType::kHealth)) {
        logger::trace("Health found");
        if (const auto health = copy_from->GetByType<RE::ExtraHealth>()) {
            if (RE::ExtraHealth* health_fake = RE::BSExtraData::Create<RE::ExtraHealth>()) {
                Copy::CopyHealth(health, health_fake);
                copy_to->Add(health_fake);
            } else return false;
        } else return false;
    }
    // Rank
    if (copy_from->HasType(RE::ExtraDataType::kRank)) {
        logger::trace("Rank found");
        if (const auto rank = skyrim_cast<RE::ExtraRank*>(copy_from->GetByType(RE::ExtraDataType::kRank))) {
            RE::ExtraRank* rank_fake = RE::BSExtraData::Create<RE::ExtraRank>();
            Copy::CopyRank(rank, rank_fake);
            copy_to->Add(rank_fake);
        } else return false;
    }
    // TimeLeft
    if (copy_from->HasType(RE::ExtraDataType::kTimeLeft)) {
        logger::trace("TimeLeft found");
        if (const auto timeleft = skyrim_cast<RE::ExtraTimeLeft*>(copy_from->GetByType(RE::ExtraDataType::kTimeLeft))) {
            RE::ExtraTimeLeft* timeleft_fake = RE::BSExtraData::Create<RE::ExtraTimeLeft>();
            Copy::CopyTimeLeft(timeleft, timeleft_fake);
            copy_to->Add(timeleft_fake);
        } else return false;
    }
    // Charge
    if (copy_from->HasType(RE::ExtraDataType::kCharge)) {
        logger::trace("Charge found");
        if (const auto charge = skyrim_cast<RE::ExtraCharge*>(copy_from->GetByType(RE::ExtraDataType::kCharge))) {
            RE::ExtraCharge* charge_fake = RE::BSExtraData::Create<RE::ExtraCharge>();
            Copy::CopyCharge(charge, charge_fake);
            copy_to->Add(charge_fake);
        } else return false;
    }
    // Scale
    if (copy_from->HasType(RE::ExtraDataType::kScale)) {
        logger::trace("Scale found");
        if (const auto scale = skyrim_cast<RE::ExtraScale*>(copy_from->GetByType(RE::ExtraDataType::kScale))) {
            RE::ExtraScale* scale_fake = RE::BSExtraData::Create<RE::ExtraScale>();
            Copy::CopyScale(scale, scale_fake);
            copy_to->Add(scale_fake);
        } else return false;
    }
    // UniqueID
    if (copy_from->HasType(RE::ExtraDataType::kUniqueID)) {
        logger::trace("UniqueID found");
        const auto uniqueid = skyrim_cast<RE::ExtraUniqueID*>(copy_from->GetByType(RE::ExtraDataType::kUniqueID));
        if (uniqueid) {
            RE::ExtraUniqueID* uniqueid_fake = RE::BSExtraData::Create<RE::ExtraUniqueID>();
            Copy::CopyUniqueID(uniqueid, uniqueid_fake);
            copy_to->Add(uniqueid_fake);
        } else return false;
    }
    // Poison
    if (copy_from->HasType(RE::ExtraDataType::kPoison)) {
        logger::trace("Poison found");
        if (const auto poison = skyrim_cast<RE::ExtraPoison*>(copy_from->GetByType(RE::ExtraDataType::kPoison))) {
            RE::ExtraPoison* poison_fake = RE::BSExtraData::Create<RE::ExtraPoison>();
            Copy::CopyPoison(poison, poison_fake);
            copy_to->Add(poison_fake);
        } else return false;
    }
    // ObjectHealth
    if (copy_from->HasType(RE::ExtraDataType::kObjectHealth)) {
        logger::trace("ObjectHealth found");
        const auto objhealth =
            skyrim_cast<RE::ExtraObjectHealth*>(copy_from->GetByType(RE::ExtraDataType::kObjectHealth));
        if (objhealth) {
            RE::ExtraObjectHealth* objhealth_fake = RE::BSExtraData::Create<RE::ExtraObjectHealth>();
            Copy::CopyObjectHealth(objhealth, objhealth_fake);
            copy_to->Add(objhealth_fake);
        } else return false;
    }
    // Light
    if (copy_from->HasType(RE::ExtraDataType::kLight)) {
        logger::trace("Light found");
        if (const auto light = skyrim_cast<RE::ExtraLight*>(copy_from->GetByType(RE::ExtraDataType::kLight))) {
            RE::ExtraLight* light_fake = RE::BSExtraData::Create<RE::ExtraLight>();
            Copy::CopyLight(light, light_fake);
            copy_to->Add(light_fake);
        } else return false;
    }
    // Radius
    if (copy_from->HasType(RE::ExtraDataType::kRadius)) {
        logger::trace("Radius found");
        if (const auto radius = skyrim_cast<RE::ExtraRadius*>(copy_from->GetByType(RE::ExtraDataType::kRadius))) {
            RE::ExtraRadius* radius_fake = RE::BSExtraData::Create<RE::ExtraRadius>();
            Copy::CopyRadius(radius, radius_fake);
            copy_to->Add(radius_fake);
        } else return false;
    }
    // Sound (Disabled)
    /*if (copy_from->HasType(RE::ExtraDataType::kSound)) {
        logger::trace("Sound found");
        auto sound = static_cast<RE::ExtraSound*>(copy_from->GetByType(RE::ExtraDataType::kSound));
        if (sound) {
            RE::ExtraSound* sound_fake = RE::BSExtraData::Create<RE::ExtraSound>();
            sound_fake->handle = sound->handle;
            copy_to->Add(sound_fake);
        } else
            RaiseMngrErr("Failed to get radius from copy_from");
    }*/
    // LinkedRef (Disabled)
    /*if (copy_from->HasType(RE::ExtraDataType::kLinkedRef)) {
        logger::trace("LinkedRef found");
        auto linkedref =
            static_cast<RE::ExtraLinkedRef*>(copy_from->GetByType(RE::ExtraDataType::kLinkedRef));
        if (linkedref) {
            RE::ExtraLinkedRef* linkedref_fake = RE::BSExtraData::Create<RE::ExtraLinkedRef>();
            linkedref_fake->linkedRefs = linkedref->linkedRefs;
            copy_to->Add(linkedref_fake);
        } else
            RaiseMngrErr("Failed to get linkedref from copy_from");
    }*/
    // Horse
    if (copy_from->HasType(RE::ExtraDataType::kHorse)) {
        logger::trace("Horse found");
        if (const auto horse = skyrim_cast<RE::ExtraHorse*>(copy_from->GetByType(RE::ExtraDataType::kHorse))) {
            RE::ExtraHorse* horse_fake = RE::BSExtraData::Create<RE::ExtraHorse>();
            Copy::CopyHorse(horse, horse_fake);
            copy_to->Add(horse_fake);
        } else return false;
    }
    // Hotkey
    if (copy_from->HasType(RE::ExtraDataType::kHotkey)) {
        logger::trace("Hotkey found");
        if (const auto hotkey = skyrim_cast<RE::ExtraHotkey*>(copy_from->GetByType(RE::ExtraDataType::kHotkey))) {
            RE::ExtraHotkey* hotkey_fake = RE::BSExtraData::Create<RE::ExtraHotkey>();
            Copy::CopyHotkey(hotkey, hotkey_fake);
            copy_to->Add(hotkey_fake);
        } else return false;
    }
    // Weapon Attack Sound (Disabled)
    /*if (copy_from->HasType(RE::ExtraDataType::kWeaponAttackSound)) {
        logger::trace("WeaponAttackSound found");
        auto weaponattacksound = static_cast<RE::ExtraWeaponAttackSound*>(
            copy_from->GetByType(RE::ExtraDataType::kWeaponAttackSound));
        if (weaponattacksound) {
            RE::ExtraWeaponAttackSound* weaponattacksound_fake =
                RE::BSExtraData::Create<RE::ExtraWeaponAttackSound>();
            weaponattacksound_fake->handle = weaponattacksound->handle;
            copy_to->Add(weaponattacksound_fake);
        } else
            RaiseMngrErr("Failed to get weaponattacksound from copy_from");
    }*/
    // Activate Ref (Disabled)
    /*if (copy_from->HasType(RE::ExtraDataType::kActivateRef)) {
        logger::trace("ActivateRef found");
        auto activateref =
            static_cast<RE::ExtraActivateRef*>(copy_from->GetByType(RE::ExtraDataType::kActivateRef));
        if (activateref) {
            RE::ExtraActivateRef* activateref_fake = RE::BSExtraData::Create<RE::ExtraActivateRef>();
            activateref_fake->parents = activateref->parents;
            activateref_fake->activateFlags = activateref->activateFlags;
        } else
            RaiseMngrErr("Failed to get activateref from copy_from");
    }*/
    // TextDisplayData
    if (copy_from->HasType(RE::ExtraDataType::kTextDisplayData)) {
        logger::trace("TextDisplayData found");
        const auto textdisplaydata =
            skyrim_cast<RE::ExtraTextDisplayData*>(copy_from->GetByType(RE::ExtraDataType::kTextDisplayData));
        if (textdisplaydata) {
            RE::ExtraTextDisplayData* textdisplaydata_fake =
                RE::BSExtraData::Create<RE::ExtraTextDisplayData>();
            Copy::CopyTextDisplayData(textdisplaydata, textdisplaydata_fake);
            copy_to->Add(textdisplaydata_fake);
        } else return false;
    }
    // Soul
    if (copy_from->HasType(RE::ExtraDataType::kSoul)) {
        logger::trace("Soul found");
        if (const auto soul = skyrim_cast<RE::ExtraSoul*>(copy_from->GetByType(RE::ExtraDataType::kSoul))) {
            RE::ExtraSoul* soul_fake = RE::BSExtraData::Create<RE::ExtraSoul>();
            Copy::CopySoul(soul, soul_fake);
            copy_to->Add(soul_fake);
        } else return false;
    }
    // Flags
    //if (copy_from->HasType(RE::ExtraDataType::kFlags)) {
    //logger::trace("Flags found");
    //if (const auto flags = skyrim_cast<RE::ExtraFlags*>(copy_from->GetByType(RE::ExtraDataType::kFlags))) {
    //    SKSE::stl::enumeration<RE::ExtraFlags::Flag, std::uint32_t> flags_fake;
    //    if (flags->flags.all(RE::ExtraFlags::Flag::kBlockActivate))
    //        flags_fake.set(RE::ExtraFlags::Flag::kBlockActivate);
    //    if (flags->flags.all(RE::ExtraFlags::Flag::kBlockPlayerActivate))
    //        flags_fake.set(RE::ExtraFlags::Flag::kBlockPlayerActivate);
    //    if (flags->flags.all(RE::ExtraFlags::Flag::kBlockLoadEvents))
    //        flags_fake.set(RE::ExtraFlags::Flag::kBlockLoadEvents);
    //    if (flags->flags.all(RE::ExtraFlags::Flag::kBlockActivateText))
    //        flags_fake.set(RE::ExtraFlags::Flag::kBlockActivateText);
    //    if (flags->flags.all(RE::ExtraFlags::Flag::kPlayerHasTaken))
    //        flags_fake.set(RE::ExtraFlags::Flag::kPlayerHasTaken);
    // RE::ExtraFlags* flags_fake = RE::BSExtraData::Create<RE::ExtraFlags>();
    // flags_fake->flags = flags->flags;
    // copy_to->Add(flags_fake);
    //} else return false;
    //}
    // Lock (Disabled)
    /*if (copy_from->HasType(RE::ExtraDataType::kLock)) {
        logger::trace("Lock found");
        auto lock = static_cast<RE::ExtraLock*>(copy_from->GetByType(RE::ExtraDataType::kLock));
        if (lock) {
            RE::ExtraLock* lock_fake = RE::BSExtraData::Create<RE::ExtraLock>();
            lock_fake->lock = lock->lock;
            copy_to->Add(lock_fake);
        } else
            RaiseMngrErr("Failed to get lock from copy_from");
    }*/
    // Teleport (Disabled)
    /*if (copy_from->HasType(RE::ExtraDataType::kTeleport)) {
        logger::trace("Teleport found");
        auto teleport =
            static_cast<RE::ExtraTeleport*>(copy_from->GetByType(RE::ExtraDataType::kTeleport));
        if (teleport) {
            RE::ExtraTeleport* teleport_fake = RE::BSExtraData::Create<RE::ExtraTeleport>();
            teleport_fake->teleportData = teleport->teleportData;
            copy_to->Add(teleport_fake);
        } else
            RaiseMngrErr("Failed to get teleport from copy_from");
    }*/
    // LockList (Disabled)
    /*if (copy_from->HasType(RE::ExtraDataType::kLockList)) {
        logger::trace("LockList found");
        auto locklist =
            static_cast<RE::ExtraLockList*>(copy_from->GetByType(RE::ExtraDataType::kLockList));
        if (locklist) {
            RE::ExtraLockList* locklist_fake = RE::BSExtraData::Create<RE::ExtraLockList>();
            locklist_fake->list = locklist->list;
            copy_to->Add(locklist_fake);
        } else
            RaiseMngrErr("Failed to get locklist from copy_from");
    }*/
    // OutfitItem (Disabled)
    /*if (copy_from->HasType(RE::ExtraDataType::kOutfitItem)) {
        logger::trace("OutfitItem found");
        auto outfititem =
            static_cast<RE::ExtraOutfitItem*>(copy_from->GetByType(RE::ExtraDataType::kOutfitItem));
        if (outfititem) {
            RE::ExtraOutfitItem* outfititem_fake = RE::BSExtraData::Create<RE::ExtraOutfitItem>();
            outfititem_fake->id = outfititem->id;
            copy_to->Add(outfititem_fake);
        } else
            RaiseMngrErr("Failed to get outfititem from copy_from");
    }*/
    // CannotWear (Disabled)
    /*if (copy_from->HasType(RE::ExtraDataType::kCannotWear)) {
        logger::trace("CannotWear found");
        auto cannotwear =
            static_cast<RE::ExtraCannotWear*>(copy_from->GetByType(RE::ExtraDataType::kCannotWear));
        if (cannotwear) {
            RE::ExtraCannotWear* cannotwear_fake = RE::BSExtraData::Create<RE::ExtraCannotWear>();
            copy_to->Add(cannotwear_fake);
        } else
            RaiseMngrErr("Failed to get cannotwear from copy_from");
    }*/
    // Ownership (OK)
    if (copy_from->HasType(RE::ExtraDataType::kOwnership)) {
        logger::trace("Ownership found");
        if (const auto ownership = skyrim_cast<RE::ExtraOwnership
            *>(copy_from->GetByType(RE::ExtraDataType::kOwnership))) {
            logger::trace("length of fake extradatalist: {}", copy_to->GetCount());
            RE::ExtraOwnership* ownership_fake = RE::BSExtraData::Create<RE::ExtraOwnership>();
            Copy::CopyOwnership(ownership, ownership_fake);
            copy_to->Add(ownership_fake);
            logger::trace("length of fake extradatalist: {}", copy_to->GetCount());
        } else return false;
    }

    return true;
}

int32_t xData::GetXDataCostOverride(const RE::ExtraDataList* xList) {
    if (!xList) return 0;
    int32_t extra_costs = 0;
    if (const auto xench = xList->GetByType<RE::ExtraEnchantment>()) {
        if (const auto ench = xench->enchantment) {
            extra_costs += FunctionsSkyrim::GetEnchantmentCostOverride(ench);
        }
    }
    return extra_costs;
}

void xData::AddTextDisplayData(RE::ExtraDataList* extraDataList, const std::string& displayName) {
    if (!extraDataList) return;
    if (extraDataList->HasType(RE::ExtraDataType::kTextDisplayData)) {
        auto* txtdisplaydata = extraDataList->GetByType<RE::ExtraTextDisplayData>();
        txtdisplaydata->SetName(displayName.c_str());
        return;
    }
    const auto textDisplayData = RE::BSExtraData::Create<RE::ExtraTextDisplayData>();
    textDisplayData->SetName(displayName.c_str());
    extraDataList->Add(textDisplayData);
}

// Credits and much love to digital-apple: https://github.com/digital-apple/ArcaneDisenchanterNG/blob/177f3d20d39ee2af28c786b251a09a6dbb4fae5e/source/System.cpp#L110
RE::ExtraDataList* xData::ConstructExtraDataList() {
    const auto memoryManager = RE::MemoryManager::GetSingleton();
    const auto alloc = memoryManager->Allocate(0x20, 0, false);
    return ConstructExtraDataList(alloc);
}

RE::ExtraDataList* xData::GetOrCreateExtraList(RE::InventoryEntryData* data, const bool a_create) {
    if (!data) {
        logger::error("GetOrCreateExtraList: data is null");
        return nullptr;
    }
    if (!data->extraLists) {
        data->extraLists = new RE::BSSimpleList<RE::ExtraDataList*>();
    }
    if (!data->extraLists->empty()) {
        return data->extraLists->front();
    }

    if (!a_create) {
        return nullptr;
    }

    auto* newList = ConstructExtraDataList();

    data->AddExtraList(newList);
    return newList;
}

bool xData::UpdateExtrasInInventory(RE::TESObjectREFR* from_ref, const FormID from_item_formid,
                                    RE::TESObjectREFR* to_ref, const FormID to_item_formid) {
    const auto from_item = RE::TESForm::LookupByID<RE::TESBoundObject>(from_item_formid);
    const auto to_item = RE::TESForm::LookupByID<RE::TESBoundObject>(to_item_formid);
    if (!from_item || !to_item || !from_ref || !to_ref) {
        logger::error("UpdateExtrasInInventory: null refs or items");
        return false;
    }

    auto from_inv = from_ref->GetInventory();
    const auto it_from = from_inv.find(from_item);
    if (it_from == from_inv.end()) {
        logger::error("Item not found in from_ref");
        return false;
    }
    const auto entry_from = it_from->second.second.get();
    if (!entry_from) {
        logger::error("Item entry_from null");
        return false;
    }
    auto to_inv = to_ref->GetInventory();
    const auto it_to = to_inv.find(to_item);
    if (it_to == to_inv.end()) {
        logger::error("Item not found in to_ref");
        return false;
    }
    const auto entry_to = it_to->second.second.get();
    if (!entry_to) {
        logger::error("Item entry_to null");
        return false;
    }

    RE::ExtraDataList* extralist_from = GetOrCreateExtraList(entry_from, false);
    if (!extralist_from) {
        return true;
    }
    RE::ExtraDataList* extralist_to = GetOrCreateExtraList(entry_to);
    if (!extralist_to) {
        logger::error("Extra data list is null (to)");
        return false;
    }

    if (!UpdateExtras(extralist_from, extralist_to)) {
        logger::error("Failed to update extras");
        return false;
    }

    return true;
}

bool Inventory::EntryHasXDataList(const RE::InventoryEntryData* entry) {
    if (entry && entry->extraLists && !entry->extraLists->empty()) return true;
    return false;
}

bool Inventory::HasItemEntry(RE::TESBoundObject* item,
                             const RE::TESObjectREFR::InventoryItemMap& inventory,
                             const bool nonzero_entry_check) {
    if (inventory.contains(item))
        return nonzero_entry_check
                   ? inventory.at(item).first > 0
                   : true;
    return false;
}

std::int32_t Inventory::GetItemCount(RE::TESBoundObject* item,
                                     const RE::TESObjectREFR::InventoryItemMap& inventory) {
    if (!HasItemEntry(item, inventory, true)) return 0;
    return inventory.find(item)->second.first;
}


std::int32_t Inventory::GetItemValue(RE::TESBoundObject* item, const RE::TESObjectREFR::InventoryItemMap& inventory) {
    if (!HasItemEntry(item, inventory, true)) return 0;
    return inventory.find(item)->second.second->GetValue();
}

bool Inventory::IsQuestItem(const FormID formid, RE::TESObjectREFR* inv_owner) {
    const auto inventory = inv_owner->GetInventory();
    if (const auto item = FormReader::GetFormByID<RE::TESBoundObject>(formid)) {
        if (const auto it = inventory.find(item); it != inventory.end()) {
            if (it->second.second->IsQuestObject()) return true;
        }
    }
    return false;
}

int32_t Inventory::GetEntryCostOverride(const RE::InventoryEntryData* entry) {
    if (!EntryHasXDataList(entry)) return 0;
    int32_t extra_costs = 0;
    for (const auto& xList : *entry->extraLists) {
        extra_costs += xData::GetXDataCostOverride(xList);
    }
    return extra_costs;
}

int Inventory::GetValueInContainer(RE::TESObjectREFR* container) {
    if (!container) {
        logger::warn("Container is null");
        return 0;
    }
    int total_value = 0;
    for (auto inventory = container->GetInventory(); auto& [fst, snd] : inventory) {
        if (snd.first <= 0) continue;
        const auto gold_value = fst->GetGoldValue();
        total_value += gold_value * snd.first;
        int extra_costs = 0;
        extra_costs += GetEntryCostOverride(snd.second.get());
        total_value += extra_costs;
    }
    return total_value;
}

void Inventory::FavoriteItem(RE::InventoryEntryData* entry_data, RE::InventoryChanges* inventory_changes) {
    const auto xLists = entry_data->extraLists;
    bool no_extra_ = false;
    if (!xLists || xLists->empty()) {
        logger::trace("No extraLists");
        no_extra_ = true;
    }
    if (no_extra_) {
        logger::trace("No extraLists");
        //inventory_changes->SetFavorite((*it), nullptr);
    } else if (xLists->front()) {
        inventory_changes->SetFavorite(entry_data, xLists->front());
    }
}

bool Inventory::IsFavorited(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner) {
    if (!item) {
        logger::warn("Item is null");
        return false;
    }
    if (!inventory_owner) {
        logger::warn("Inventory owner is null");
        return false;
    }
    auto inventory = inventory_owner->GetInventory();
    if (const auto it = inventory.find(item); it != inventory.end()) {
        if (it->second.first <= 0) logger::warn("Item count is 0");
        return it->second.second->IsFavorited();
    }
    return false;
}

namespace {
    void EquipItem_(RE::Actor* a_actor, RE::TESBoundObject* a_item, RE::ExtraDataList* a_xlist) {
        /*SKSE::GetTaskInterface()->AddTask([a_actor,a_item,a_xlist] {
        }
        );*/
        RE::ActorEquipManager::GetSingleton()->EquipObject(
            a_actor, a_item, a_xlist, 1,
            nullptr, true, false, false, false);
    }

    void UnequipItem_(RE::Actor* a_actor, RE::TESBoundObject* a_item, RE::ExtraDataList* a_xlist) {
        /*SKSE::GetTaskInterface()->AddTask([a_actor,a_item,a_xlist] {
        }
        );*/
        RE::ActorEquipManager::GetSingleton()->UnequipObject(
            a_actor, a_item, a_xlist, 1,
            nullptr, true, false, false, false);
    }
}

void Inventory::EquipItem(const RE::InventoryEntryData* entry_data, const bool unequip) {
    const auto xLists = entry_data->extraLists;
    if (!entry_data || !xLists) {
        logger::error("Item extraLists is null");
        return;
    }

    const auto player_ref = RE::PlayerCharacter::GetSingleton();
    const auto a_bound = entry_data->object;
    const auto a_func_ptr = unequip ? &UnequipItem_ : &EquipItem_;
    const auto a_list = xLists->empty() ? nullptr : xLists->front();

    a_func_ptr(player_ref, a_bound, a_list);
}

bool Inventory::IsEquipped(RE::TESBoundObject* item) {
    logger::trace("IsEquipped");

    if (!item) {
        logger::trace("Item is null");
        return false;
    }

    const auto player_ref = RE::PlayerCharacter::GetSingleton();
    auto inventory = player_ref->GetInventory();
    if (const auto it = inventory.find(item); it != inventory.end()) {
        if (it->second.first <= 0) logger::warn("Item count is 0");
        return it->second.second->IsWorn();
    }
    return false;
}

void Inventory::ToggleEquip(RE::TESBoundObject* item) {
    const auto player = RE::PlayerCharacter::GetSingleton();
    if (IsEquipped(item)) {
        RE::ActorEquipManager::GetSingleton()->UnequipObject(player, item);
    } else {
        RE::ActorEquipManager::GetSingleton()->EquipObject(player, item);
    }
}

void WorldObject::SwapObjects(RE::TESObjectREFR* a_from, RE::TESBoundObject* a_to, const bool apply_havok) {
    if (!a_from) {
        logger::error("Ref is null.");
        return;
    }
    const auto ref_base = a_from->GetBaseObject();
    if (!ref_base) {
        logger::error("Ref base is null.");
        return;
    }
    if (!a_to) {
        logger::error("Base is null.");
        return;
    }
    if (ref_base->GetFormID() == a_to->GetFormID()) {
        logger::trace("Ref and base are the same.");
        return;
    }
    a_from->SetObjectReference(a_to);
    if (!apply_havok) return;
    SKSE::GetTaskInterface()->AddTask([a_from]() {
        a_from->Disable();
        a_from->Enable(false);
    });
}

void Math::LinAlg::R3::rotateX(RE::NiPoint3& v, const float angle) {
    const float y = v.y * cos(angle) - v.z * sin(angle);
    const float z = v.y * sin(angle) + v.z * cos(angle);
    v.y = y;
    v.z = z;
}

void Math::LinAlg::R3::rotateY(RE::NiPoint3& v, const float angle) {
    const float x = v.x * cos(angle) + v.z * sin(angle);
    const float z = -v.x * sin(angle) + v.z * cos(angle);
    v.x = x;
    v.z = z;
}

void Math::LinAlg::R3::rotateZ(RE::NiPoint3& v, const float angle) {
    const float x = v.x * cos(angle) - v.y * sin(angle);
    const float y = v.x * sin(angle) + v.y * cos(angle);
    v.x = x;
    v.y = y;
}

void Math::LinAlg::R3::rotate(RE::NiPoint3& v, const float angleX, const float angleY, const float angleZ) {
    rotateX(v, angleX);
    rotateY(v, angleY);
    rotateZ(v, angleZ);
}


void xData::Copy::CopyEnchantment(const RE::ExtraEnchantment* from, RE::ExtraEnchantment* to) {
    logger::trace("CopyEnchantment");
    to->enchantment = from->enchantment;
    to->charge = from->charge;
    to->removeOnUnequip = from->removeOnUnequip;
}

void xData::Copy::CopyHealth(const RE::ExtraHealth* from, RE::ExtraHealth* to) {
    logger::trace("CopyHealth");
    to->health = from->health;
}

void xData::Copy::CopyRank(const RE::ExtraRank* from, RE::ExtraRank* to) {
    logger::trace("CopyRank");
    to->rank = from->rank;
}

void xData::Copy::CopyTimeLeft(const RE::ExtraTimeLeft* from, RE::ExtraTimeLeft* to) {
    logger::trace("CopyTimeLeft");
    to->time = from->time;
}

void xData::Copy::CopyCharge(const RE::ExtraCharge* from, RE::ExtraCharge* to) {
    logger::trace("CopyCharge");
    to->charge = from->charge;
}

void xData::Copy::CopyScale(const RE::ExtraScale* from, RE::ExtraScale* to) {
    logger::trace("CopyScale");
    to->scale = from->scale;
}

void xData::Copy::CopyUniqueID(const RE::ExtraUniqueID* from, RE::ExtraUniqueID* to) {
    logger::trace("CopyUniqueID");
    to->baseID = from->baseID;
    to->uniqueID = from->uniqueID;
}

void xData::Copy::CopyPoison(const RE::ExtraPoison* from, RE::ExtraPoison* to) {
    logger::trace("CopyPoison");
    to->poison = from->poison;
    to->count = from->count;
}

void xData::Copy::CopyObjectHealth(const RE::ExtraObjectHealth* from, RE::ExtraObjectHealth* to) {
    logger::trace("CopyObjectHealth");
    to->health = from->health;
}

void xData::Copy::CopyLight(const RE::ExtraLight* from, RE::ExtraLight* to) {
    logger::trace("CopyLight");
    to->lightData = from->lightData;
}

void xData::Copy::CopyRadius(const RE::ExtraRadius* from, RE::ExtraRadius* to) {
    logger::trace("CopyRadius");
    to->radius = from->radius;
}

void xData::Copy::CopyHorse(const RE::ExtraHorse* from, RE::ExtraHorse* to) {
    logger::trace("CopyHorse");
    to->horseRef = from->horseRef;
}

void xData::Copy::CopyHotkey(const RE::ExtraHotkey* from, RE::ExtraHotkey* to) {
    logger::trace("CopyHotkey");
    to->hotkey = from->hotkey;
}

void xData::Copy::CopyTextDisplayData(const RE::ExtraTextDisplayData* from, RE::ExtraTextDisplayData* to) {
    to->displayName = from->displayName;
    to->displayNameText = from->displayNameText;
    to->ownerQuest = from->ownerQuest;
    to->ownerInstance = from->ownerInstance;
    to->temperFactor = from->temperFactor;
    to->customNameLength = from->customNameLength;
}

void xData::Copy::CopySoul(const RE::ExtraSoul* from, RE::ExtraSoul* to) {
    logger::trace("CopySoul");
    to->soul = from->soul;
}

void xData::Copy::CopyOwnership(const RE::ExtraOwnership* from, RE::ExtraOwnership* to) {
    logger::trace("CopyOwnership");
    to->owner = from->owner;
}

void MsgBoxesNotifs::SkyrimMessageBox::Show(const std::string& bodyText,
                                            const std::vector<std::string>& buttonTextValues,
                                            std::function<void(unsigned int)> callback) {
    const auto* factoryManager = RE::MessageDataFactoryManager::GetSingleton();
    const auto* uiStringHolder = RE::InterfaceStrings::GetSingleton();
    auto* factory = factoryManager->GetCreator<RE::MessageBoxData>(
        uiStringHolder->messageBoxData);
    auto* messagebox = factory->Create();
    const RE::BSTSmartPointer<RE::IMessageBoxCallback> messageCallback = RE::make_smart<
        MessageBoxResultCallback>(callback);
    messagebox->callback = messageCallback;
    messagebox->bodyText = bodyText;
    for (auto& text : buttonTextValues) messagebox->buttonText.push_back(text.c_str());
    RE::MessageBoxMenu::QueueMessage(messagebox);
}

std::string_view Menu::CloseMenu() {
    const auto uiManager = RE::UI::GetSingleton();
    if (const auto inventoryMenu = uiManager->GetMenu<RE::InventoryMenu>()) {
        RE::UIMessageQueue::GetSingleton()->AddMessage(RE::InventoryMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide,
                                                       nullptr);
        RE::UIMessageQueue::GetSingleton()->AddMessage(RE::TweenMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
        return RE::InventoryMenu::MENU_NAME;
    }

    if (const auto favoritesMenu = uiManager->GetMenu<RE::FavoritesMenu>()) {
        RE::UIMessageQueue::GetSingleton()->AddMessage(RE::FavoritesMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide,
                                                       nullptr);
        return RE::FavoritesMenu::MENU_NAME;
    }

    if (const auto containerMenu = uiManager->GetMenu<RE::ContainerMenu>()) {
        RE::UIMessageQueue::GetSingleton()->AddMessage(RE::ContainerMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide,
                                                       nullptr);
        //RE::UIMessageQueue::GetSingleton()->AddMessage(RE::DialogueMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
        return RE::ContainerMenu::MENU_NAME;
    }
    static std::string_view empty_menuname;
    return empty_menuname;
}

bool Menu::IsOpen(const RE::BSFixedString& menu_name) {
    if (const auto ui = RE::UI::GetSingleton()) {
        return ui->IsMenuOpen(menu_name);
    }
    return false;
}

void Menu::OpenMenu(const std::string_view menuname) {
    if (menuname.empty()) return;
    if (IsOpen(menuname)) return;
    const RE::BSFixedString menuName(menuname);
    if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
        queue->AddMessage(menuName, RE::UI_MESSAGE_TYPE::kShow, nullptr);
    }
}

bool Menu::GetContainerMenuOwner(RE::TESObjectREFRPtr& a_out) {
    if (const auto ui = RE::UI::GetSingleton(); ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
        return RE::LookupReferenceByHandle(RE::ContainerMenu::GetTargetRefHandle(), a_out);
    }
    return false;
}

RE::RefHandle Menu::GetOwnerInContainerMenu(const RE::FormID a_itemid) {
    #undef GetObject
    const auto ui = RE::UI::GetSingleton();
    if (const auto container_menu = ui->GetMenu<RE::ContainerMenu>()) {
        if (const auto itemlist = container_menu->GetRuntimeData().itemList) {
            for (const auto& a_item : itemlist->items) {
                if (a_item) {
                    const auto& a_data = a_item->data;
                    if (a_data.objDesc && a_data.objDesc->GetObject()->GetFormID() == a_itemid) {
                        return a_data.owner;
                    }
                }
            }
        }
    }

    return {};
}

bool Menu::IsPickpocketingOrStealing() {
    if (const auto container_menu = RE::UI::GetSingleton()->GetMenu<RE::ContainerMenu>()) {
        if (static_cast<int>(container_menu->GetContainerMode()) % 3) {
            return true;
        }
    }
    return false;
}

void Menu::UpdateItemList() {
    if (const auto ui = RE::UI::GetSingleton(); ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
        UpdateItemList<RE::InventoryMenu>();
    } else if (ui->IsMenuOpen(RE::BarterMenu::MENU_NAME)) {
        UpdateItemList<RE::BarterMenu>();
    } else if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
        UpdateItemList<RE::ContainerMenu>();
    }
}

void ModCompatibility::MakeChecks() {
    Settings::po3installed = Mods::IsPo3Installed();
}

void ModCompatibility::Load() {
    if (const auto data_handler = RE::TESDataHandler::GetSingleton()) {
        Mods::ui_extensions_installed = data_handler->LookupModByName("UIExtensions.esp") != nullptr;

        for (const auto local_id : Mods::doppelgangers_local) {
            if (const auto a_form = data_handler->LookupForm(local_id, Mods::doppelgangers_path)) {
                Mods::doppelgangers.insert(a_form->GetFormID());
            }
        }
    }
}