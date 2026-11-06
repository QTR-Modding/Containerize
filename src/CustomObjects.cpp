#include "CustomObjects.h"

bool FormRefID::operator<(const FormRefID& other) const {
    return outerKey < other.outerKey || (outerKey == other.outerKey && innerKey < other.innerKey);
}

FormIDX::FormIDX(const FormID id, const bool value1, const bool value2, std::string value3) : id(id), equipped(value1),
    favorited(value2), name(std::move(
        value3)) {
}

bool FormRefIDX::operator<(const FormRefIDX& other) const {
    // Here, you can access the boolean values via outerKey
    return outerKey.id < other.outerKey.id ||
           (outerKey.id == other.outerKey.id && innerKey < other.innerKey);
}

bool FormFormID::operator<(const FormFormID& other) const {
    return outerKey < other.outerKey || (outerKey == other.outerKey && innerKey < other.innerKey);
}

Source::Source(const std::uint32_t id, std::string id_str, const float capacity, const float cs,
               const bool a_transfer_on_use) :
    weight_ratio(1 - cs), capacity(capacity), transfer_on_use(a_transfer_on_use), formid(id),
    editorid(std::move(id_str)) {
    if (!formid) {
        if (const auto form = RE::TESForm::LookupByEditorID(editorid)) formid = form->GetFormID();
        else logger::info("Could not find formid for editorid {}", editorid);
    } else if (editorid.empty()) {
        if (const auto form = FormReader::GetFormByID(formid)) editorid = clib_util::editorID::get_editorID(form);
        else logger::info("Could not find form for formid {:x}", formid);
        if (editorid.empty())
            logger::info(
                "Could not find editorid for formid {:x}. Make sure you have the latest version of po3's tweaks.",
                formid);
    }
    weight_ratio = std::clamp(1.f - cs, 0.f, 1.f);
}

inline std::string_view Source::GetName() const {
    if (const auto* form = FormReader::GetFormByID(formid, editorid)) return form->GetName();
    return "";
}

std::string Source::GetWeightText(const float weight) const {
    std::ostringstream stream1;
    stream1 << std::fixed << std::setprecision(2) << weight;
    std::ostringstream stream2;
    stream2 << std::fixed << std::setprecision(2) << capacity;
    return fmt::format("{}/{}", stream1.str(), stream2.str());
}

RE::TESBoundObject* Source::GetBoundObject() const {
    return FormReader::GetFormByID<RE::TESBoundObject>(formid, editorid);
}

void Source::AddInitialItem(const FormID form_id, const Count count) {
    logger::trace("Adding initial item with formid: {} and count: {}", form_id, count);
    initial_items[form_id] = count;
}

Count Source::CanBeAdded(const RE::TESBoundObject* item, const Count count, RE::TESObjectREFR* chest) const {
    constexpr float minimum_item_weight = 0.001f;
    constexpr float minimum_weight_ratio = 0.00001f;
    if (item->GetWeight() < minimum_item_weight) return count;
    if (weight_ratio < minimum_weight_ratio) return count;
    if (capacity == 0.f) return count;
    const auto remaining_capacity = capacity - chest->GetWeightInContainer() * weight_ratio;
    const auto item_weight = item->GetWeight() * weight_ratio;
    const auto can_be_added = static_cast<Count>(remaining_capacity / (item_weight + EPSILON));
    return std::max(0, std::min(can_be_added, count));
}

std::string_view Source::SkyPromptSettings::GetLabel(const std::string_view name, const std::string_view fallback) const {
    const auto it = labels.find(name);
    return it != labels.end() ? std::string_view(it->second) : fallback;
}

bool Source::IsHealthy() const {
    if (formid == 0) return false;
    if (editorid.empty()) return false;
    if (const auto* form = FormReader::GetFormByID(formid, editorid); !form) return false;
    if (capacity < 0.f) return false;
    if (weight_ratio < 0.f || weight_ratio > 1.f) return false;

    return std::ranges::all_of(initial_items, [](const auto& pair) {
        const auto& [a_formid, a_count] = pair;
        return a_count > 0 && FormReader::GetFormByID(a_formid);
    });
}