#include "SkyPrompt.h"
#include "MainManager.h"
#include "Animations.h"
#include "Events.h"
#include "Hooks.h"
#include "Manager.h"
#include "CLibUtilsQTR/Tasker.hpp"
#include "ClibUtil/distribution.hpp"

using namespace SkyPrompt;

namespace {
    std::string requested_theme;

    bool ApplyTheme(const std::string& theme) {
        if (requested_theme == theme) return false;
        if (!SkyPromptAPI::RequestTheme(g_clientID, theme)) (void)SkyPromptAPI::RequestTheme(g_clientID, {});
        requested_theme = theme;
        return true;
    }

    void UpdatePrompts(const SkyPromptAPI::PromptSink* sink, std::vector<SkyPromptAPI::Prompt>& prompts,
                       const Source::SkyPromptSettings& settings,
                       const std::initializer_list<SkyPromptAPI::Prompt> candidates) {
        const bool theme_changed = ApplyTheme(settings.theme);
        auto visible = candidates | std::views::filter([](const auto& prompt) { return !prompt.text.empty(); });
        if (!prompts.empty() && (theme_changed || !std::ranges::equal(prompts, visible, [](const auto& a, const auto& b) {
            return a.eventID == b.eventID && a.refid == b.refid;
        }))) SkyPromptAPI::RemovePrompt(sink, g_clientID);
        prompts.assign(visible.begin(), visible.end());
    }
}

void SkyPrompt::LoadHandshakes() {
    constexpr auto folder = "Data/SKSE/Plugins/Containerize/handshakes";
    constexpr SkyPromptAPI::HandshakeKey containerize = 0x4354525A;
    if (!std::filesystem::is_directory(folder)) {
        return;
    }
    for (const auto& filename : clib_util::distribution::get_configs(folder, "", ".txt")) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            logger::error("Could not read handshake file '{}'.", filename);
            continue;
        }
        std::string line;
        while (std::getline(file, line)) {
            clib_util::string::trim(line);
            if (line.empty()) {
                continue;
            }
            std::string_view value(line);
            constexpr std::string_view prefix = "0x";
            const bool hex = value.starts_with(prefix) || value.starts_with("0X");
            if (hex) {
                value.remove_prefix(prefix.size());
            }
            SkyPromptAPI::HandshakeKey key{};
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), key, hex ? 16 : 10);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
                logger::error("Invalid handshake key '{}' in '{}'.", line, filename);
                continue;
            }
            if (!SkyPromptAPI::RequestHandshake(g_clientID, containerize, key)) {
                logger::error("Could not register handshake key '{}' from '{}'.", line, filename);
            }
        }
    }
}

void RefPromptSink::Start(RE::TESObjectREFR* a_ref) {
    weight_text.clear();
    value_text.clear();
    const auto manager = MainManager::GetSingleton();
    const auto source = manager->GetSource(a_ref);
    if (source->token_mode) {
        SkyPromptAPI::RemovePrompt(this, g_clientID);
        prompts.clear();
        return;
    }
    const auto& settings = source->skyprompt;
    const RefID refid = settings.attach ? a_ref->GetFormID() : 0;
    open_prompt.text = settings.GetLabel("open", Strings::open_bag);
    if (open_prompt.text.starts_with('$')) {
        std::string translated;
        if (SKSE::Translation::Translate(std::string(open_prompt.text), translated) &&
            translated.find_first_not_of(" \t\r\n\f\v") == std::string::npos) {
            open_prompt.text = {};
        }
    }
    rename_prompt.text = manager->CanRename() ? settings.GetLabel("rename", Strings::rename_bag) : "";

    if (const auto label = settings.GetLabel("weight", Strings::weight); !label.empty()) {
        if (const auto text = manager->GetWeightText(a_ref); !text.empty()) weight_text.append(label).append(" ").append(text);
    }
    if (const auto label = settings.GetLabel("value", Strings::value); !label.empty()) {
        if (const auto text = manager->GetValueText(a_ref); !text.empty()) value_text.append(label).append(" ").append(text);
    }
    weight_prompt.text = weight_text;
    value_prompt.text = value_text;
    for (auto prompt : {&open_prompt, &rename_prompt, &weight_prompt, &value_prompt}) prompt->refid = refid;
    UpdatePrompts(this, prompts, settings, {open_prompt, rename_prompt, weight_prompt, value_prompt});
}

void RefPromptSink::Stop() { prompts.clear(); }

void RefPromptSink::ProcessEvent(const SkyPromptAPI::PromptEvent event) const {
    if (event.type) {
        return;
    }
    EventSink::RemovePrompts();

    if (const auto crosshairref = RE::CrosshairPickData::GetSingleton()->target) {
        if (const auto a_ref = crosshairref->get().get()) {
            if (const auto prompt_eventid = event.prompt.eventID; prompt_eventid == 0) {
                if (MainManager::GetSingleton()->IsTokenMode(a_ref)) return;
                const auto duration = Animations::SetUpAnimationOnOpen(a_ref, true);
                MainManager::GetSingleton()->OnActivateContainer(a_ref, 0, duration, true);
            } else if (prompt_eventid == 1) {
                MainManager::GetSingleton()->OnActivateContainer(a_ref, 1);
            }
        }
    }
}

bool SkyPrompt::IsAnyMenuOpen() {
    const auto ui = RE::UI::GetSingleton();
    for (const auto a_name : blockedMenus) {
        if (ui->IsMenuOpen(a_name)) {
            return true;
        }
    }
    return false;
}

void MenuPromptSink::ProcessEvent(const SkyPromptAPI::PromptEvent event) const {
    #undef GetObject
    if (event.type) {
        return;
    }
    EventSink::RemoveMenuPrompts();
    if (const auto a_entry = Hooks::GetSelectedEntryInMenu()) {
        if (const auto a_fake = a_entry->GetObject();
            a_fake && Manager::GetSingleton()->IsFakeContainer(a_fake->GetFormID())) {
            if (event.prompt.eventID == 1) {
                MainManager::RenameCallback(a_fake);
                return;
            }
            OnOpen(a_fake, RE::UI::GetSingleton()->IsMenuOpen(RE::InventoryMenu::MENU_NAME) && a_entry->IsWorn());
        }
    }
}

void MenuPromptSink::Show(const RE::TESBoundObject* a_fake) const {
    weight_text.clear();

    if (const auto a_ref = RE::Inventory3DManager::GetSingleton()->tempRef) {
        const auto refid = a_ref->GetFormID();
        open_prompt.refid = refid;
        rename_prompt.refid = refid;
        weight_prompt.refid = refid;
    } else {
        open_prompt.refid = 0;
        rename_prompt.refid = 0;
        weight_prompt.refid = 0;
    }

    if (Manager::GetSingleton()->IsTokenMode(a_fake->GetFormID())) {
        Hide();
        return;
    }
    const auto* manager = Manager::GetSingleton();
    const auto real = manager->FakeToRealContainer(a_fake->GetFormID());
    const auto& settings = manager->GetContainerSource(real->GetFormID())->skyprompt;
    open_prompt.text = settings.GetLabel("open", Strings::open_bag);
    rename_prompt.text = ModCompatibility::Mods::ui_extensions_installed
        ? settings.GetLabel("rename", Strings::rename_bag) : "";
    if (const auto label = settings.GetLabel("weight", Strings::weight); !label.empty()) {
        if (const auto text = Manager::GetSingleton()->GetWeightText(a_fake); !text.empty()) weight_text.append(label).append(" ").append(text);
    }
    weight_prompt.text = weight_text;
    UpdatePrompts(this, prompts, settings, {open_prompt, rename_prompt, weight_prompt});

    if (!SkyPromptAPI::SendPrompt(this, g_clientID)) {
    }
}

void MenuPromptSink::Hide() const {
    SkyPromptAPI::RemovePrompt(this, g_clientID);
    prompts.clear();
}

void MenuPromptSink::OnOpen(RE::TESBoundObject* a_fake, const bool is_worn) {
    using namespace ModCompatibility::Mods;

    const auto manager = Manager::GetSingleton();
    if (manager->isUninstalled.load() || manager->IsTokenMode(a_fake->GetFormID())) return;

    const auto a_real = manager->FakeToRealContainer(a_fake->GetFormID());

    if (is_worn) {
        Hooks::OnIsWorn(a_fake);
    }

    MainManager::GetSingleton()->CloseMenu();

    MainManager::GetSingleton()->OnLongPressEquip(a_fake, Animations::SetUpAnimationOnOpen(a_real, is_worn));
}

void RegistrationPromptSink::ProcessEvent(const SkyPromptAPI::PromptEvent event) const {
    if (event.type) {
        return;
    }

    EventSink::RemoveMenuPrompts();

    if (const auto a_entry = Hooks::GetSelectedEntryInMenu()) {
        const bool is_in_inventory_menu = RE::UI::GetSingleton()->IsMenuOpen(RE::InventoryMenu::MENU_NAME);
        const bool is_worn = is_in_inventory_menu && a_entry->IsWorn();
        const auto real_id = a_entry->GetObject()->GetFormID();
        const auto owner_handle = Menu::GetOwnerInContainerMenu(real_id);
        RE::TESObjectREFRPtr a_owner;
        if (is_in_inventory_menu || !RE::LookupReferenceByHandle(owner_handle, a_owner)) {
            a_owner.reset();
        }
        if (const auto a_fake = Manager::GetSingleton()->RegisterFromMenu(a_entry, a_owner.get());
            a_fake && Manager::GetSingleton()->IsFakeContainer(a_fake->GetFormID())) {
            if (event.prompt.eventID == 4) {
                MainManager::RenameCallback(a_fake);
                return;
            }
            clib_utilsQTR::Tasker::GetSingleton()->PushTask([a_fake, is_worn] {
                                                                SKSE::GetTaskInterface()->AddTask([a_fake, is_worn] {
                                                                    MenuPromptSink::OnOpen(a_fake, is_worn);
                                                                });
                                                            }, 150
                );
        }
    }
}

void RegistrationPromptSink::Show(const RE::TESBoundObject* a_item) const {
    weight_text.clear();

    if (const auto a_ref = RE::Inventory3DManager::GetSingleton()->tempRef) {
        const auto refid = a_ref->GetFormID();
        open_prompt.refid = refid;
        rename_prompt.refid = refid;
        weight_prompt.refid = refid;
    } else {
        open_prompt.refid = 0;
        rename_prompt.refid = 0;
        weight_prompt.refid = 0;
    }

    if (Manager::GetSingleton()->IsTokenMode(a_item->GetFormID())) {
        Hide();
        return;
    }
    const auto* manager = Manager::GetSingleton();
    const auto& settings = manager->GetContainerSource(a_item->GetFormID())->skyprompt;
    open_prompt.text = settings.GetLabel("open", Strings::open_bag);
    rename_prompt.text = ModCompatibility::Mods::ui_extensions_installed
        ? settings.GetLabel("rename", Strings::rename_bag) : "";
    if (const auto label = settings.GetLabel("weight", Strings::weight); !label.empty()) {
        if (const auto text = Manager::GetSingleton()->GetWeightText(a_item); !text.empty()) weight_text.append(label).append(" ").append(text);
    }
    weight_prompt.text = weight_text;
    UpdatePrompts(this, prompts, settings, {open_prompt, rename_prompt, weight_prompt});

    if (!SkyPromptAPI::SendPrompt(this, g_clientID)) {
    }
}

void RegistrationPromptSink::Hide() const {
    SkyPromptAPI::RemovePrompt(this, g_clientID);
    prompts.clear();
    containermenu_owner.reset();
}