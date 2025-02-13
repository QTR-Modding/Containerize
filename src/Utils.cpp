#include <Utils.h>
#include <numbers>


bool GetDllVersion(const std::wstring& dllPath, DWORD& major, DWORD& minor, DWORD& build, DWORD& revision)
{
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

std::wstring s2ws(const std::string& str)
{
    const int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), nullptr, 0);
    std::wstring wstrTo( size_needed, 0 );
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), wstrTo.data(), size_needed);
    return wstrTo;
}

bool IsPo3Installed()
{
    if (!std::filesystem::exists(po3path)) {
		logger::error("Po3's Tweaks is not installed.");
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

void SetupLog()
{
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

std::filesystem::path GetLogPath()
{
    const auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
    auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
    return logFilePath;
}

std::vector<std::string> ReadLogFile()
{
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

std::string DecodeTypeCode(const std::uint32_t typeCode)
{
	char buf[4];
    buf[3] = char(typeCode);
    buf[2] = char(typeCode >> 8);
    buf[1] = char(typeCode >> 16);
    buf[0] = char(typeCode >> 24);
    return std::string(buf, buf + 4);
}


bool isValidHexWithLength7or8(const char* input)
{
    std::string inputStr(input);

    if (inputStr.substr(0, 2) == "0x") {
        // Remove "0x" from the beginning of the string
        inputStr = inputStr.substr(2);
    }

    const std::regex hexRegex("^[0-9A-Fa-f]{7,8}$");  // Allow 7 to 8 characters
    const bool isValid = std::regex_match(inputStr, hexRegex);
    return isValid;
}

std::string GetEditorID(const FormID a_formid) {
    if (const auto form = RE::TESForm::LookupByID(a_formid)) return clib_util::editorID::get_editorID(form);
    return "";
}

FormID GetFormEditorIDFromString(const std::string& formEditorId)
{
    if (formEditorId.empty()) return 0;
    if (isValidHexWithLength7or8(formEditorId.c_str())) {
        int form_id_;
        std::stringstream ss;
        ss << std::hex << formEditorId;
        ss >> form_id_;
        if (const auto temp_form = GetFormByID(form_id_)) return temp_form->GetFormID();
        logger::warn("Formid is null for (valid hex) form-/editorid {}", formEditorId);
        //return 0;
    }
    //if (!IsPo3Installed()) {
    //    logger::error("Po3 is not installed.");
    //    MsgBoxesNotifs::Windows::Po3ErrMsg();
    //    return 0;
    //}
    if (const auto temp_form = GetFormByID(0, formEditorId)) return temp_form->GetFormID();
    //logger::info("Formid is null for editorid {}", formEditorId);
    return 0;
}

std::string String::trim(const std::string& str) { 
    // Find the first non-whitespace character from the beginning
    const size_t start = str.find_first_not_of(" \t\n\r");

    // If the string is all whitespace, return an empty string
    if (start == std::string::npos) return "";

    // Find the last non-whitespace character from the end
    const size_t end = str.find_last_not_of(" \t\n\r");

    // Return the substring containing the trimmed characters
    return str.substr(start, end - start + 1);
}

std::string String::toLowercase(const std::string& str) {
    std::string result = str;
    std::ranges::transform(result, result.begin(),
                           [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::string String::replaceLineBreaksWithSpace(const std::string& input) {
    std::string result = input;
    std::ranges::replace(result, '\n', ' ');
    return result;
}

bool String::includesWord(const std::string& input, const std::vector<std::string>& strings) {
    std::string lowerInput = toLowercase(input);
    lowerInput = replaceLineBreaksWithSpace(lowerInput);
    lowerInput = trim(lowerInput);
    lowerInput = " " + lowerInput + " ";  // Add spaces to the beginning and end of the string

    for (const auto& str : strings) {
        std::string lowerStr = str;
        lowerStr = trim(lowerStr);
        lowerStr = " " + lowerStr + " ";  // Add spaces to the beginning and end of the string
        std::ranges::transform(lowerStr, lowerStr.begin(),
                               [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });

        // logger::trace("lowerInput: {} lowerStr: {}", lowerInput, lowerStr);

        if (lowerInput.find(lowerStr) != std::string::npos) {
            return true;  // The input string includes one of the strings
        }
    }
    return false;  // None of the strings in 'strings' were found in the input string
}

std::size_t FunctionsSkyrim::GetExtraDataListLength(const RE::ExtraDataList* dataList) {
    std::size_t length = 0;
    for (auto it = dataList->begin(); it != dataList->end(); ++it) {
        // Increment the length for each element in the list
        ++length;
    }
    return length;
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


bool xData::UpdateExtras(RE::ExtraDataList* copy_from, RE::ExtraDataList* copy_to)
{
    if (!copy_from || !copy_to) return false;
    // Enchantment
    if (copy_from->HasType(RE::ExtraDataType::kEnchantment)) {
        logger::trace("Enchantment found");
        const auto enchantment =
            skyrim_cast<RE::ExtraEnchantment*>(copy_from->GetByType(RE::ExtraDataType::kEnchantment));
        if (enchantment) {
            RE::ExtraEnchantment* enchantment_fake = RE::BSExtraData::Create<RE::ExtraEnchantment>();
            // log the associated actor value
            logger::trace("Associated actor value: {}", enchantment->enchantment->GetAssociatedSkill());
            Copy::CopyEnchantment(enchantment, enchantment_fake);
            copy_to->Add(enchantment_fake);
        } else return false;
    }
    // Health
    if (copy_from->HasType(RE::ExtraDataType::kHealth)) {
        logger::trace("Health found");
        if (const auto health = skyrim_cast<RE::ExtraHealth*>(copy_from->GetByType(RE::ExtraDataType::kHealth))) {
            RE::ExtraHealth* health_fake = RE::BSExtraData::Create<RE::ExtraHealth>();
            Copy::CopyHealth(health, health_fake);
            copy_to->Add(health_fake);
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
    // Flags (OK)
    if (copy_from->HasType(RE::ExtraDataType::kFlags)) {
        logger::trace("Flags found");
        if (const auto flags = skyrim_cast<RE::ExtraFlags*>(copy_from->GetByType(RE::ExtraDataType::kFlags))) {
            SKSE::stl::enumeration<RE::ExtraFlags::Flag, std::uint32_t> flags_fake;
            if (flags->flags.all(RE::ExtraFlags::Flag::kBlockActivate))
                flags_fake.set(RE::ExtraFlags::Flag::kBlockActivate);
            if (flags->flags.all(RE::ExtraFlags::Flag::kBlockPlayerActivate))
                flags_fake.set(RE::ExtraFlags::Flag::kBlockPlayerActivate);
            if (flags->flags.all(RE::ExtraFlags::Flag::kBlockLoadEvents))
                flags_fake.set(RE::ExtraFlags::Flag::kBlockLoadEvents);
            if (flags->flags.all(RE::ExtraFlags::Flag::kBlockActivateText))
                flags_fake.set(RE::ExtraFlags::Flag::kBlockActivateText);
            if (flags->flags.all(RE::ExtraFlags::Flag::kPlayerHasTaken))
                flags_fake.set(RE::ExtraFlags::Flag::kPlayerHasTaken);
            // RE::ExtraFlags* flags_fake = RE::BSExtraData::Create<RE::ExtraFlags>();
            // flags_fake->flags = flags->flags;
            // copy_to->Add(flags_fake);
        } else return false;
    }
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
        if (const auto ownership = skyrim_cast<RE::ExtraOwnership*>(copy_from->GetByType(RE::ExtraDataType::kOwnership))) {
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
        const auto ench = xench->enchantment;
        auto temp_costoverride = ench->data.costOverride;
        if (temp_costoverride < 0) temp_costoverride = static_cast<int32_t>(ench->CalculateTotalGoldValue());
        if (temp_costoverride < 0)
            temp_costoverride =
                static_cast<int32_t>(ench->CalculateTotalGoldValue(RE::PlayerCharacter::GetSingleton()));
        if (temp_costoverride > 0) {
            logger::trace("CostOverride: {}", temp_costoverride);
            extra_costs += temp_costoverride;
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

bool Inventory::EntryHasXDataList(const RE::InventoryEntryData* entry) {
    if (entry && entry->extraLists && !entry->extraLists->empty()) return true;
    return false;
}

bool Inventory::HasItemEntry(RE::TESBoundObject* item, const RE::TESObjectREFR::InventoryItemMap& inventory,
                             const bool nonzero_entry_check) {
    if (inventory.contains(item)) return nonzero_entry_check ? inventory.at(item).first > 0 : true;
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

bool Inventory::IsQuestItem(const FormID formid, RE::TESObjectREFR* inv_owner)
{
    const auto inventory = inv_owner->GetInventory();
    if (const auto item = GetFormByID<RE::TESBoundObject>(formid)) {
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

void Inventory::FavoriteItem(const RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner)
{
    if (!item) return;
    if (!inventory_owner) return;
    const auto inventory_changes = inventory_owner->GetInventoryChanges();
    const auto entries = inventory_changes->entryList;
    for (auto it = entries->begin(); it != entries->end(); ++it) {
        if (!(*it)) {
			logger::error("Item entry is null");
			continue;
		}
        const auto object = (*it)->object;
        if (!object) {
			logger::error("Object is null");
			continue;
		}
        const auto formid = object->GetFormID();
        if (!formid) logger::critical("Formid is null");
        if (formid == item->GetFormID()) {
            logger::trace("Favoriting item: {}", item->GetName());
            const auto xLists = (*it)->extraLists;
            bool no_extra_ = false;
            if (!xLists || xLists->empty()) {
				logger::trace("No extraLists");
                no_extra_ = true;
			}
            logger::trace("asdasd");
            if (no_extra_) {
                logger::trace("No extraLists");
                //inventory_changes->SetFavorite((*it), nullptr);
            } else if (xLists->front()) {
                logger::trace("ExtraLists found");
                inventory_changes->SetFavorite((*it), xLists->front());
            }
            return;
        }
    }
    logger::error("Item not found in inventory");
}

bool Inventory::IsFavorited(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner)
{
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

void Inventory::EquipItem(const RE::TESBoundObject* item, const bool unequip)
{
    logger::trace("EquipItem");

    if (!item) {
        logger::error("Item is null");
        return;
    }
    const auto player_ref = RE::PlayerCharacter::GetSingleton();
    const auto inventory_changes = player_ref->GetInventoryChanges();
    const auto entries = inventory_changes->entryList;
    for (auto it = entries->begin(); it != entries->end(); ++it) {
        if (const auto formid = (*it)->object->GetFormID(); formid == item->GetFormID()) {
            if (!(*it) || !(*it)->extraLists) {
				logger::error("Item extraLists is null");
				return;
			}
            if (unequip) {
                if ((*it)->extraLists->empty()) {
                    RE::ActorEquipManager::GetSingleton()->UnequipObject(
                        player_ref, (*it)->object, nullptr, 1,
                        (const RE::BGSEquipSlot*)nullptr, true, false, false);
                } else if ((*it)->extraLists->front()) {
                    RE::ActorEquipManager::GetSingleton()->UnequipObject(
                        player_ref, (*it)->object, (*it)->extraLists->front(), 1,
                        (const RE::BGSEquipSlot*)nullptr, true, false, false);
                }
            }
            else {
                if ((*it)->extraLists->empty()) {
                    RE::ActorEquipManager::GetSingleton()->EquipObject(
                        player_ref, (*it)->object, nullptr, 1,
                        (const RE::BGSEquipSlot*)nullptr, true, false, false, false);
                } else if ((*it)->extraLists->front()) {
                    RE::ActorEquipManager::GetSingleton()->EquipObject(
                        player_ref, (*it)->object, (*it)->extraLists->front(), 1,
                        (const RE::BGSEquipSlot*)nullptr, true, false, false, false);
                }
            }
            return;
        }
    }
}

bool Inventory::IsEquipped(RE::TESBoundObject* item)
{
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

void Inventory::ToggleEquip(RE::TESBoundObject* item)
{
	const auto player = RE::PlayerCharacter::GetSingleton();
    if (Inventory::IsEquipped(item)) {
		RE::ActorEquipManager::GetSingleton()->UnequipObject(player,item);
	}
	else {
		RE::ActorEquipManager::GetSingleton()->EquipObject(player, item);
	}
}

RE::TESObjectREFR* WorldObject::DropObjectIntoTheWorld(RE::TESBoundObject* obj, const Count count, const bool player_owned)
{
    const auto player_ch = RE::PlayerCharacter::GetSingleton();

    constexpr auto multiplier = 100.0f;
    constexpr float q_pi = std::numbers::pi_v<float>;
    auto orji_vec = RE::NiPoint3{multiplier, 0.f, player_ch->GetHeight()};
    Math::LinAlg::R3::rotateZ(orji_vec, q_pi / 4.f - player_ch->GetAngleZ());
    const auto drop_pos = player_ch->GetPosition() + orji_vec;
    const auto player_cell = player_ch->GetParentCell();
    const auto player_ws = player_ch->GetWorldspace();
    if (!player_cell && !player_ws) {
        logger::critical("Player cell AND player world is null.");
        return nullptr;
    }
    auto* newPropRef =
        RE::TESDataHandler::GetSingleton()
                            ->CreateReferenceAtLocation(obj, drop_pos, {0.0f, 0.0f, 0.0f}, player_cell,
                                                        player_ws, nullptr, nullptr, {}, false, false)
            .get()
            .get();
    if (!newPropRef) {
        logger::critical("New prop ref is null.");
        return nullptr;
    }
    if (player_owned) newPropRef->extraList.SetOwner(RE::TESForm::LookupByID(0x07));
	if (count > 1) newPropRef->extraList.SetCount(static_cast<uint16_t>(count));
    return newPropRef;
}

void WorldObject::SwapObjects(RE::TESObjectREFR* a_from, RE::TESBoundObject* a_to, const bool apply_havok)
{
    logger::trace("SwapObjects");
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
    /*a_from->Disable();
    a_from->Enable(false);*/

    /*float afX = 100;
    float afY = 100;
    float afZ = 100;
    float afMagnitude = 100;*/
    /*auto args = RE::MakeFunctionArguments(std::move(afX), std::move(afY), std::move(afZ),
    std::move(afMagnitude)); vm->DispatchMethodCall(object, "ApplyHavokImpulse", args, callback);*/
    // Looked up here (wSkeever): https:  // www.nexusmods.com/skyrimspecialedition/mods/73607
    /*SKSE::GetTaskInterface()->AddTask([a_from]() {
        ApplyHavokImpulse(a_from, 0.f, 0.f, 10.f, 5000.f);
    });*/
    //SKSE::GetTaskInterface()->AddTask([a_from]() {
    //    // auto player_ch = RE::PlayerCharacter::GetSingleton();
    //    // player_ch->StartGrabObject();
    //    auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    //    auto policy = vm->GetObjectHandlePolicy();
    //    auto handle = policy->GetHandleForObject(a_from->GetFormType(), a_from);
    //    RE::BSTSmartPointer<RE::BSScript::Object> object;
    //    vm->CreateObject2("ObjectReference", object);
    //    if (!object) logger::warn("Object is null");
    //    vm->BindObject(object, handle, false);
    //    auto args = RE::MakeFunctionArguments(std::move(0.f), std::move(0.f), std::move(1.f), std::move(5.f));
    //    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
    //    if (vm->DispatchMethodCall(object, "ApplyHavokImpulse", args, callback)) logger::trace("FUSRODAH");
    //});
}

void Math::LinAlg::R3::rotateX(RE::NiPoint3& v, const float angle)
{
    const float y = v.y * cos(angle) - v.z * sin(angle);
    const float z = v.y * sin(angle) + v.z * cos(angle);
    v.y = y;
    v.z = z;
}

void Math::LinAlg::R3::rotateY(RE::NiPoint3& v, const float angle)
{
    const float x = v.x * cos(angle) + v.z * sin(angle);
    const float z = -v.x * sin(angle) + v.z * cos(angle);
    v.x = x;
    v.z = z;
}

void Math::LinAlg::R3::rotateZ(RE::NiPoint3& v, const float angle)
{
    const float x = v.x * cos(angle) - v.y * sin(angle);
    const float y = v.x * sin(angle) + v.y * cos(angle);
    v.x = x;
    v.y = y;
}

void Math::LinAlg::R3::rotate(RE::NiPoint3& v, const float angleX, const float angleY, const float angleZ)
{
    rotateX(v, angleX);
	rotateY(v, angleY);
	rotateZ(v, angleZ);
}


void xData::Copy::CopyEnchantment(const RE::ExtraEnchantment* from, RE::ExtraEnchantment* to)
{
    logger::trace("CopyEnchantment");
	to->enchantment = from->enchantment;
	to->charge = from->charge;
	to->removeOnUnequip = from->removeOnUnequip;
}

void xData::Copy::CopyHealth(const RE::ExtraHealth* from, RE::ExtraHealth* to)
{
    logger::trace("CopyHealth");
    to->health = from->health;
}

void xData::Copy::CopyRank(const RE::ExtraRank* from, RE::ExtraRank* to)
{
    logger::trace("CopyRank");
	to->rank = from->rank;
}

void xData::Copy::CopyTimeLeft(const RE::ExtraTimeLeft* from, RE::ExtraTimeLeft* to)
{
    logger::trace("CopyTimeLeft");
    to->time = from->time;
}

void xData::Copy::CopyCharge(const RE::ExtraCharge* from, RE::ExtraCharge* to)
{
    logger::trace("CopyCharge");
	to->charge = from->charge;
}

void xData::Copy::CopyScale(const RE::ExtraScale* from, RE::ExtraScale* to)
{
    logger::trace("CopyScale");
	to->scale = from->scale;
}

void xData::Copy::CopyUniqueID(const RE::ExtraUniqueID* from, RE::ExtraUniqueID* to)
{
    logger::trace("CopyUniqueID");
    to->baseID = from->baseID;
    to->uniqueID = from->uniqueID;
}

void xData::Copy::CopyPoison(const RE::ExtraPoison* from, RE::ExtraPoison* to)
{
    logger::trace("CopyPoison");
	to->poison = from->poison;
	to->count = from->count;
}

void xData::Copy::CopyObjectHealth(const RE::ExtraObjectHealth* from, RE::ExtraObjectHealth* to)
{
    logger::trace("CopyObjectHealth");
	to->health = from->health;
}

void xData::Copy::CopyLight(const RE::ExtraLight* from, RE::ExtraLight* to)
{
    logger::trace("CopyLight");
    to->lightData = from->lightData;
}

void xData::Copy::CopyRadius(const RE::ExtraRadius* from, RE::ExtraRadius* to)
{
    logger::trace("CopyRadius");
	to->radius = from->radius;
}

void xData::Copy::CopyHorse(const RE::ExtraHorse* from, RE::ExtraHorse* to)
{
    logger::trace("CopyHorse");
    to->horseRef = from->horseRef;
}

void xData::Copy::CopyHotkey(const RE::ExtraHotkey* from, RE::ExtraHotkey* to)
{
    logger::trace("CopyHotkey");
	to->hotkey = from->hotkey;
}

void xData::Copy::CopyTextDisplayData(const RE::ExtraTextDisplayData* from, RE::ExtraTextDisplayData* to)
{
    to->displayName = from->displayName;
    to->displayNameText = from->displayNameText;
    to->ownerQuest = from->ownerQuest;
    to->ownerInstance = from->ownerInstance;
    to->temperFactor = from->temperFactor;
    to->customNameLength = from->customNameLength;
}

void xData::Copy::CopySoul(const RE::ExtraSoul* from, RE::ExtraSoul* to)
{
    logger::trace("CopySoul");
    to->soul = from->soul;
}

void xData::Copy::CopyOwnership(const RE::ExtraOwnership* from, RE::ExtraOwnership* to)
{
    logger::trace("CopyOwnership");
	to->owner = from->owner;
}

void MsgBoxesNotifs::SkyrimMessageBox::Show(const std::string& bodyText, const std::vector<std::string>& buttonTextValues, std::function<void(unsigned int)> callback)
{
    const auto* factoryManager = RE::MessageDataFactoryManager::GetSingleton();
    const auto* uiStringHolder = RE::InterfaceStrings::GetSingleton();
    auto* factory = factoryManager->GetCreator<RE::MessageBoxData>(
        uiStringHolder->messageBoxData);  // "MessageBoxData" <--- can we just use this string?
    auto* messagebox = factory->Create();
    const RE::BSTSmartPointer<RE::IMessageBoxCallback> messageCallback = RE::make_smart<MessageBoxResultCallback>(callback);
    messagebox->callback = messageCallback;
    messagebox->bodyText = bodyText;
    for (auto& text : buttonTextValues) messagebox->buttonText.push_back(text.c_str());
    messagebox->QueueMessage();
}

std::string_view Menu::CloseMenu()
{
	const auto uiManager = RE::UI::GetSingleton();
    if (const auto inventoryMenu = uiManager->GetMenu<RE::InventoryMenu>()) {
        RE::UIMessageQueue::GetSingleton()->AddMessage("InventoryMenu", RE::UI_MESSAGE_TYPE::kHide, nullptr);
        RE::UIMessageQueue::GetSingleton()->AddMessage("TweenMenu", RE::UI_MESSAGE_TYPE::kHide, nullptr);
		return RE::InventoryMenu::MENU_NAME;
    }

    if (const auto favoritesMenu = uiManager->GetMenu<RE::FavoritesMenu>()) {
        RE::UIMessageQueue::GetSingleton()->AddMessage("FavoritesMenu", RE::UI_MESSAGE_TYPE::kHide, nullptr);
		return RE::FavoritesMenu::MENU_NAME;
    }

    if (const auto containerMenu = uiManager->GetMenu<RE::ContainerMenu>()) {
        RE::UIMessageQueue::GetSingleton()->AddMessage("ContainerMenu", RE::UI_MESSAGE_TYPE::kHide, nullptr);
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
