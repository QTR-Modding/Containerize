#pragma once
#include "DynamicFormTracker.h"
#include "ClibUtil/singleton.hpp"
#include <shared_mutex>

class Manager final : public SaveLoadData,
public clib_util::singleton::ISingleton<Manager>
{
    // private variables

    bool uiextensions_is_present = false;
    RE::TESObjectREFR* player_ref = RE::PlayerCharacter::GetSingleton()->As<RE::TESObjectREFR>();
    //RE::EffectSetting* empty_mgeff = nullptr;
    
    //  maybe i dont need this by using uniqueID for new forms
    // runtime specific
    std::map<RefID,FormFormID> ChestToFakeContainer; // chest refid -> {real container formid (outerKey), fake container formid (innerKey)}
    RE::TESObjectREFR* current_container = nullptr;

    // unowned stuff
    const RefID unownedChestOGRefID = 0x000EA29A;
    const RefID unownedChestFormID = 0x000EA299;
    RE::TESObjectCELL* unownedCell = nullptr;
    RE::TESObjectCONT* unownedChest = nullptr;
    //RE::TESObjectCELL* unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000FE47B);  // cwquartermastercontainers
    //RE::TESObjectCONT* unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(0x000A0DB5); // playerhousechestnew
    const RE::NiPoint3 unownedChestPos = {1986.f, 1780.f, 6784.f};
    
    std::vector<FormID> external_favs; // runtime specific, FormIDs of fake containers if faved
    std::vector<RefID> handled_external_conts; // runtime specific to prevent unnecessary checks in HandleFakePlacement
    std::map<FormID,std::string> renames;  // runtime specific, custom names for fake containers
    std::pair<RE::TESBoundObject*, RefID> real_to_sendback = {nullptr,0};  // pff
    std::pair<RE::TESBoundObject*, RefID> queued_real_to_sendback = {nullptr,0};  // pff
    std::string closed_menu;
	RE::TESObjectREFRPtr containermenu_owner = nullptr;

    std::set<FormID> doppelgangers_local = {0x832,0x833,0x834,0x835,0x836,0x837,0x838,0x839,0x83a,0x83b};

    mutable std::shared_mutex source_mutex_;
	mutable std::shared_mutex chest2fake_mutex_;

    void SendReal(RE::TESBoundObject* real_obj, RE::TESObjectREFR* chest);

    std::string GetChestName(const RE::TESObjectREFR* chest) const;

    [[nodiscard]] bool ActivateChest(RE::TESObjectREFR* chest) const;

    [[nodiscard]] static int GetChestValue(RE::TESObjectREFR* a_chest);

    // from container out in the world to linked chest
    [[nodiscard]] RE::TESObjectREFR* GetContainerChest(const RE::TESObjectREFR* a_container) const;

    [[nodiscard]] uint32_t GetNoChests() const;

    [[nodiscard]] std::vector<RefID> GetConnectedChests(RefID chestID);

    [[nodiscard]] bool IsUnownedChest(RefID refid) const;

    [[nodiscard]] RE::TESObjectREFR* MakeChest(RE::NiPoint3 Pos3 = {0.0f, 0.0f, 0.0f}) const;

    [[nodiscard]] RE::TESObjectREFR* AddChest(uint32_t chest_no) const;

    [[nodiscard]] RE::TESObjectREFR* FindNotMatchedChest() const;

    std::vector<FormID> RemoveAllItemsFromChest(RE::TESObjectREFR* chest, RE::TESObjectREFR* move2ref = nullptr);

    void DeRegisterChest(RefID chest_ref);

    // OK. from real container formid to linked source
    [[nodiscard]] const Source* GetContainerSource(FormID real_id) const;
    [[nodiscard]] Source* GetContainerSource(FormID real_id);

    // returns true only if the item is in the inventory with positive count. removes the item if it is in the inventory with 0 count
    [[nodiscard]] static bool HasItemPlusCleanUp(RE::TESBoundObject* item, RE::TESObjectREFR* item_owner);

    // removes only one unit of the item
    static RE::ObjectRefHandle RemoveItem(RE::TESObjectREFR* moveFrom, RE::TESObjectREFR* moveTo, RE::TESBoundObject* a_item,
                                          RE::ITEM_REMOVE_REASON reason);

    [[nodiscard]] static bool PickUpItem(RE::TESObjectREFR* item, unsigned int max_try = 3);

    // Removes the object from the world and adds it to an inventory
    [[nodiscard]] static bool MoveObject(RE::TESObjectREFR* ref, RE::TESObjectREFR* move2container, bool owned = true);

    template <typename T>
    void UpdateFakeWV(T* fake_form, RE::TESObjectREFR* chest_linked, float weight_ratio);

    // Updates weight and value of fake container and uses Copy and applies renaming
    void UpdateFakeWV(RE::TESBoundObject* fake_form, RE::TESObjectREFR* chest_linked, float weight_ratio);


    [[nodiscard]] static bool UpdateExtrasInInventory(RE::TESObjectREFR* from_inv, FormID from_item_formid,
                                                      RE::TESObjectREFR* to_inv, FormID to_item_formid);

    void HandleFormDelete_(RefID chest_refid);


    std::vector<Source> sources;

    void RaiseMngrErr(const std::string& err_msg_ = "Error");

    void InitFailed();

    template <typename T>
    FormID CreateFakeContainer(T* realcontainer, RefID connected_chest, RE::ExtraDataList*);

    // Creates new form for fake container // pre 0.7.1: and adds it to unownedChestOG
    FormID CreateFakeContainer(RE::TESBoundObject* container, RefID connected_chest, RE::ExtraDataList* el);

    // for the cases when real container is in its chest and fake container is in some other inventory (player,unownedchest,external_container)
    // DOES NOT UPDATE THE SOURCE DATA and CHESTTOFAKECONTAINER !!!
    void qTRICK_(SourceDataKey chest_ref, SourceDataVal cont_ref,bool fake_nonexistent = false);

    // places fakes according to loaded data to player or unowned chests
    void FakePlacementCeption(RefID chest_ref, std::vector<RefID>& ha);

    void FakePlacement(RefID saved_ref, RefID chest_ref, RE::TESObjectREFR* external_cont = nullptr);

    void RemoveCarryWeightBoost(FormID item_formid, RE::TESObjectREFR* inventory_owner);

    bool HandleRegistration(RE::TESObjectREFR* a_container);

    void RenameCallback();

    template <typename T>
    static void Rename(const std::string& new_name, T item) {
        logger::trace("Rename");
        if (!item) logger::warn("Item not found");
        else item->fullName = new_name;
    }

public:

    std::set<FormID> doppelgangers;
	std::atomic<bool> isUninstalled = false;

    const char* GetType() override { return "Manager"; }
    void Init();

    void MsgBoxCallback(int result);

    [[nodiscard]] RefID GetContainerChestID(RefID container_refid) const;
    [[nodiscard]] RefID GetFakeContainerChestID(FormID fake_id) const;
    RE::TESBoundObject* GetFakeBound(RefID chest_id) const;
    FormID GetFakeID(RefID chest_id) const;
    FormID GetRealID(RefID chest_id) const;
    void OnPickup(RE::TESObjectREFR* picked_up_by, RE::TESObjectREFR * a_object);
	void HandleDrop(RE::TESObjectREFR* fake_object);
    void UpdateData(RefID chestID, RefID loc_id);
    void OnLongPressEquip(const RE::TESBoundObject* a_selected_item);
	void UpdateFakeWV(RE::TESBoundObject* fake_form);
    Count CanBeAdded(const RE::TESBoundObject* a_item, Count a_count, const RE::TESBoundObject* fake_container);
    [[nodiscard]] RE::TESBoundObject* FakeToRealContainer(FormID fake) const;


    void OnActivateContainer(RE::TESObjectREFR* a_container, int msgbox_action);

    // places fake objects in external containers after load game
    void HandleFakePlacement(RE::TESObjectREFR* external_cont);

    [[nodiscard]] bool IsFakeContainer(FormID formid);

    // Checks if realcontainer_formid is in the sources
    [[nodiscard]] bool IsRealContainer(FormID formid) const;

    // Checks if ref has formid in the sources
    [[nodiscard]] bool IsRealContainer(const RE::TESObjectREFR* ref) const;

    void RenameContainer(const std::string& new_name);

    void OnContainerMenuExit();
    void OnContainerMenuEnter();

    [[nodiscard]] bool IsARegistry(RefID registry) const;

    void HandleCraftingExit();

    void OnConsume(FormID fake_formid, RE::TESObjectREFR* consumed_by);

    void HandleSell(FormID fake_container, RE::TESObjectREFR* sell_ref);

    void HandleFormDelete(RefID refid);

    // checks if the refid is in the ChestToFakeContainer, i.e. if it is an unownedchest
    [[nodiscard]] bool IsChest(const RefID chest_refid) const { return ChestToFakeContainer.contains(chest_refid); }

    void Reset();

    void Print();

    void SendData();

    void ReceiveData();

	const std::vector<Source>& GetSources() const { return sources; }

    void Uninstall();

    RE::TESBoundObject* GetFakeBound(const RE::TESObjectREFR* a_container) const;
    std::string GetWeightText(const RE::TESObjectREFR* a_container) const;
    std::string GetValueText(const RE::TESObjectREFR* a_container) const;
};

template <typename T>
void Manager::UpdateFakeWV(T* fake_form, RE::TESObjectREFR* chest_linked, const float weight_ratio) {

    // assumes base container is already in the chest
    if (!chest_linked || !fake_form) return RaiseMngrErr("Failed to get chest.");
    const auto fake_formid = fake_form->GetFormID();
    auto real_container = FakeToRealContainer(fake_formid);
    fake_form->Copy(real_container->As<T>());
    if (renames.contains(fake_formid)) fake_form->fullName = renames.at(fake_form->GetFormID());

    FunctionsSkyrim::FormTraits<T>::SetWeight(fake_form, weight_ratio*chest_linked->GetWeightInContainer() + (1-weight_ratio) * real_container->GetWeight()); // dont change (1-weight_ratio)

    const auto chest_inventory = chest_linked->GetInventory();

    // get the ench costoverride of fake in player inventory
    int x_0 = real_container->GetGoldValue();
    const int target_value = Inventory::GetValueInContainer(chest_linked);

    if (other_settings[Settings::otherstuffKeys[3]]) {
		if (auto temp_entry = chest_inventory.find(real_container); temp_entry != chest_inventory.end()) {
			const auto extracost = Inventory::EntryHasXDataList(temp_entry->second.second.get()) ? xData::GetXDataCostOverride(temp_entry->second.second->extraLists->front()) : 0;
			x_0 = target_value - extracost;
		}
    }
    x_0 = std::max(x_0, 0);

    FunctionsSkyrim::FormTraits<T>::SetValue(fake_form, x_0);
        
    if (!Inventory::HasItem(fake_form, player_ref) || x_0 == 0) return;

    const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_form->GetFormID());
    if (!fake_bound) return RaiseMngrErr("Fake bound is null");
    const int f_0 = Inventory::GetItemValue(fake_bound, player_ref->GetInventory());
    int f_search = f_0;

    // do binary search to find the correct value up to a tolerance
    constexpr float tolerance = 0.01f; // 1%
    const float tolerance_val = std::max(2.0f, std::floor(tolerance * static_cast<float>(target_value)) + 1);  // at least 2
    constexpr int max_iter = 1000;
    int curr_iter = max_iter;

    int lower_bound = 0;
    int upper_bound = x_0;
    int x_search = (lower_bound + upper_bound) / 2;

    while (static_cast<float>(std::abs(f_search - target_value)) > tolerance_val && curr_iter > 0) {
        FunctionsSkyrim::FormTraits<T>::SetValue(fake_form, x_search);
		f_search = Inventory::GetItemValue(fake_bound, player_ref->GetInventory());

        logger::trace("x_search: {}, f_search: {}", x_search, f_search);

        if (f_search > target_value) upper_bound = x_search;
        else lower_bound = x_search;

        const int new_x_search = (lower_bound + upper_bound) / 2;
        if (new_x_search == x_search) break;

        x_search = new_x_search;
        curr_iter--;
    }

    if (curr_iter == 0) {
        logger::warn("Max iterations reached.");
        if (std::abs(f_search - target_value) > std::abs(f_0 - target_value)){
            logger::warn("Could not find a better value for fake form");
            return FunctionsSkyrim::FormTraits<T>::SetValue(fake_form, x_0);
        }
    }
}

template <typename T>
FormID Manager::CreateFakeContainer(T* realcontainer, const RefID connected_chest, RE::ExtraDataList*) {
    const auto real_container_formid = realcontainer->GetFormID();
    const auto real_container_editorid = clib_util::editorID::get_editorID(realcontainer);
    if (real_container_editorid.empty()) {
        RaiseMngrErr(std::format("Failed to get editorid of real container {} with formid {}.", realcontainer->GetName(), real_container_formid));
        return 0;
    }
    const auto new_form_id = DynamicFormTracker::GetSingleton()->FetchCreate<T>(real_container_formid, real_container_editorid, connected_chest);
    T* new_form = RE::TESForm::LookupByID<T>(new_form_id);

    if (!new_form) {
        RaiseMngrErr("Failed to create new form.");
        return 0;
    }
    new_form->fullName = realcontainer->GetFullName();
    logger::info("Created form with type: {}, Base ID: {:x}, Name: {}",
                 RE::FormTypeToString(new_form->GetFormType()), new_form_id, new_form->GetName());
    return new_form_id;
}