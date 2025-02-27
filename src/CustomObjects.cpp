#include "CustomObjects.h"

bool FormRefID::operator<(const FormRefID& other) const {
    return outerKey < other.outerKey || (outerKey == other.outerKey && innerKey < other.innerKey);
}

FormIDX::FormIDX(const FormID id, const bool value1, const bool value2, const std::string& value3): id(id), equipped(value1), favorited(value2), name(value3) {}

bool FormRefIDX::operator<(const FormRefIDX& other) const {
    // Here, you can access the boolean values via outerKey
    return outerKey.id < other.outerKey.id ||
           (outerKey.id == other.outerKey.id && innerKey < other.innerKey);
}

bool FormFormID::operator<(const FormFormID& other) const {
    return outerKey < other.outerKey || (outerKey == other.outerKey && innerKey < other.innerKey);
}

Source::Source(const std::uint32_t id, const std::string id_str, const float capacity, const float cs): capacity(capacity), formid(id), editorid(id_str), weight_ratio(1 - cs) {
    logger::trace("Creating source with formid: {:x}, editorid: {}, capacity: {}, cloud storage: {}", formid, editorid, capacity, cs);
    if (!formid) {
        logger::trace("Formid is not found. Attempting to find formid for editorid {}.", editorid);
        if (const auto form = RE::TESForm::LookupByEditorID(editorid)) formid = form->GetFormID();
        else logger::info("Could not find formid for editorid {}", editorid);
    }
	else if (editorid.empty()) {
		logger::trace("Editorid is not found. Attempting to find editorid for formid {:x}.", formid);
		if (const auto form = GetFormByID(formid)) editorid = clib_util::editorID::get_editorID(form);
		else logger::info("Could not find form for formid {:x}", formid);
		if (editorid.empty()) logger::info("Could not find editorid for formid {:x}. Make sure you have the latest version of po3's tweaks.", formid);
        
	}
    weight_ratio = std::clamp(1.f - cs, 0.f, 1.f);
}

inline std::string_view Source::GetName() const {
    logger::trace("Getting name for formid: {:x}", formid);
    if (const auto* form = GetFormByID(formid, editorid)) return form->GetName();
    return "";
}

RE::TESBoundObject* Source::GetBoundObject() const {return GetFormByID<RE::TESBoundObject>(formid, editorid);}

void Source::AddInitialItem(const FormID form_id, const Count count) {
    logger::trace("Adding initial item with formid: {} and count: {}", form_id, count);
    initial_items[form_id] = count;
}

bool Source::IsHealthy() const
{
	if (formid == 0) return false;
	if (editorid.empty()) return false;
	if (const auto* form = GetFormByID(formid, editorid); !form) return false;
	if (capacity < 0.f) return false;
	if (weight_ratio < 0.f || weight_ratio > 1.f) return false;
	for (const auto& [formid_, count] : initial_items) {
		if (count <= 0) return false;
		if (!GetFormByID(formid_)) return false;
	}
    return true;
}
