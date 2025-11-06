#pragma once
#include "DynamicFormTracker.h"
#include "ClibUtil/singleton.hpp"
#include <shared_mutex>

class Manager final : public SaveLoadData,
public clib_util::singleton::ISingleton<Manager>
{
    // private variables

    RE::TESObjectREFR* player_ref = RE::PlayerCharacter::GetSingleton()->As<RE::TESObjectREFR>();
    //RE::EffectSetting* empty_mgeff = nullptr;
    
    // runtime specific
    std::map<RefID,FormFormID> ChestToFakeContainer; // chest refid -> {real container formid (outerKey), fake container formid (innerKey)}

	// unowned stuff
    RE::TESObjectCELL* unownedCell = nullptr;
    RE::TESObjectCONT* unownedChest = nullptr;
    
    std::vector<FormID> external_favs; // runtime specific, FormIDs of fake containers if faved
    std::vector<RefID> handled_external_conts; // runtime specific to prevent unnecessary checks in HandleFakePlacement
    std::map<FormID,std::string> renames;  // runtime specific, custom names for fake containers
    std::set<RefID> reals_to_takeback = {};
    std::set<RefID> queued_chests = {}; // allows opening chest within another chest instead of reopening the original menu
    std::string closed_menu;
	RE::TESObjectREFRPtr containermenu_owner = nullptr;

    mutable std::shared_mutex source_mutex_;
	mutable std::shared_mutex chest2fake_mutex_;

    std::set<std::pair<RefID,FormID>> bypass_CanBeAdded;

    void TakeBackReal(RE::TESBoundObject* real_obj, RE::TESObjectREFR* chest);

    std::string GetChestName(const RE::TESObjectREFR* chest) const;

    [[nodiscard]] bool ActivateChest(RE::TESObjectREFR* chest) const;

    [[nodiscard]] static int GetChestValue(RE::TESObjectREFR* a_chest);

    [[nodiscard]] RE::TESObjectREFR* GetContainerChest(const RE::TESObjectREFR* a_container) const;
    [[nodiscard]] RE::TESObjectREFR* GetFakeContainerChest(const RE::TESBoundObject* a_fake) const;
    [[nodiscard]] RE::TESObjectREFR* GetContainerLocation(FormID a_fake_id) const;

    [[nodiscard]] uint32_t GetNoChests() const;

    // chestID nin icindeki chestler
    [[nodiscard]] std::vector<RefID> GetChildChests(RefID chestID, bool deep);

    [[nodiscard]] bool IsUnownedChest(RefID refid) const;

    [[nodiscard]] RE::TESObjectREFR* MakeChest(RE::NiPoint3 Pos3 = {0.0f, 0.0f, 0.0f}) const;

    [[nodiscard]] RE::TESObjectREFR* AddChest(uint32_t chest_no) const;

    [[nodiscard]] RE::TESObjectREFR* FindNotMatchedChest() const;

    std::vector<FormID> RemoveAllItemsFromChest(RE::TESObjectREFR* chest, RE::TESObjectREFR* move2ref = nullptr);

    void DeRegisterChest(RefID chest_ref);

    void OpenChestFromMenu(RE::TESObjectREFR* a_chest);

    [[nodiscard]] const Source* GetContainerSource(FormID real_id) const;
    [[nodiscard]] Source* GetContainerSource(FormID real_id);

    // returns true only if the item is in the inventory with positive count. removes the item if it is in the inventory with 0 count.
    // do I need this?
    [[nodiscard]] static bool HasItemPlusCleanUp(RE::TESBoundObject* item, RE::TESObjectREFR* item_owner);

    // removes only one unit of the item
	template <typename T>
    static RE::ObjectRefHandle RemoveItem(T* moveFrom, RE::TESObjectREFR* moveTo, RE::TESBoundObject* a_item,
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
    void FakePlacement_Sub(SourceDataKey chest_ref, SourceDataVal cont_ref,bool fake_nonexistent = false);

    // places fakes according to loaded data to player or unowned chests
    void FakePlacementCeption(RefID chest_ref, std::vector<RefID>& ha);

    void FakePlacement(RefID saved_ref, RefID chest_ref, RE::TESObjectREFR* external_cont = nullptr);

    void RemoveCarryWeightBoost(FormID item_formid, RE::TESObjectREFR* inventory_owner);

    bool HandleRegistration(RE::TESObjectREFR* a_item);

    std::string GetWeightText_(RE::TESObjectREFR* a_chest);

    static std::string GetWeightText(float weight, float capacity);

public:

	std::atomic<bool> isUninstalled = false;

    const char* GetType() override { return "Manager"; }
    void Init();

    void Gateway(int result, const RE::ObjectRefHandle& a_current_container);

    [[nodiscard]] RefID GetContainerChestID(RefID container_refid) const;
    [[nodiscard]] RefID GetFakeContainerChestID(FormID fake_id) const;
    RE::TESBoundObject* GetFakeBound(RefID chest_id) const;
    RE::TESBoundObject* GetRealBound(RefID chest_id) const;
    FormID GetFakeID(RefID chest_id) const;
    FormID GetRealID(RefID chest_id) const;
    void OnPickup(RE::TESObjectREFR* picked_up_by, RE::TESObjectREFR * a_object);
	void HandleDrop(RE::TESObjectREFR* fake_object);
    void UpdateData(RefID chestID, RefID loc_id);
    void OnLongPressEquip(const RE::TESBoundObject* a_fake, int delay=0);
	void UpdateFakeWV(RE::TESBoundObject* fake_form);
    Count CanBeAdded(const RE::TESBoundObject* a_item, Count a_count, RefID a_chestID);
    [[nodiscard]] RE::TESBoundObject* FakeToRealContainer(FormID fake) const;

    void OnActivateContainer(RE::TESObjectREFR* a_container, int msgbox_action, int a_delay=0);

    // places fake objects in external containers after load game
    void HandleFakePlacement(RE::TESObjectREFR* external_cont);

    [[nodiscard]] bool IsFakeContainer(FormID formid);

    // Checks if realcontainer_formid is in the sources
    [[nodiscard]] bool IsRealContainer(FormID formid) const;

    // Checks if ref has formid in the sources
    [[nodiscard]] bool IsRealContainer(const RE::TESObjectREFR* ref) const;

    void RenameContainer(const std::string& new_name, RE::TESBoundObject* a_fake);

    void OnChestExit(RE::TESObjectREFR* a_chest);
    void OnChestEnter(RE::TESObjectREFR* a_chest);

    [[nodiscard]] bool IsARegistry(RefID registry) const;

    void HandleCraftingExit();

    void OnConsume(FormID fake_formid, RE::TESObjectREFR* consumed_by);

    void HandleSell(FormID fake_container, RE::TESObjectREFR* sell_ref);

    void HandleFormDelete(RefID refid);

    // checks if the refid is in the ChestToFakeContainer, i.e. if it is an unownedchest
    [[nodiscard]] bool IsChest(const RefID a_refid) const { return ChestToFakeContainer.contains(a_refid); }

    void Reset();

    void Print();

    void SendData();

    void ReceiveData();

	const std::vector<Source>& GetSources() const { return sources; }

    void Uninstall();

    RE::TESBoundObject* GetFakeBound(const RE::TESObjectREFR* a_container) const;
    std::string GetWeightText(RE::TESObjectREFR* a_container);
    std::string GetWeightText(const RE::TESBoundObject* fake_or_real);
    std::string GetValueText(RE::TESObjectREFR* a_container);
    void CloseMenu();
    RE::TESBoundObject* RegisterFromMenu(RE::InventoryEntryData* a_real_entry, RE::TESObjectREFR* a_owner);
    bool IsInChestMenu() const {return !reals_to_takeback.empty();}
    bool IsChestMenuQueued() const {return !queued_chests.empty();}
    static void RenameCallback(RE::TESBoundObject* a_fake);

    template <typename T>
    static void Rename(const std::string& new_name, T item) {
        if (!item) logger::warn("Item not found");
        else item->fullName = new_name;
    }
};

template<typename T>
RE::ObjectRefHandle Manager::RemoveItem(T* moveFrom, RE::TESObjectREFR* moveTo, RE::TESBoundObject* a_item, RE::ITEM_REMOVE_REASON reason)
{
    auto ref_handle = RE::ObjectRefHandle();

    if (!moveFrom) {
        logger::critical("moveFrom is null!");
        return ref_handle;
    }
    if (moveTo && moveFrom->GetFormID() == moveTo->GetFormID()) {
        logger::info("moveFrom and moveTo are the same!");
        return ref_handle;
    }

	const RE::TESObjectREFR::InventoryItemMap inventory = moveFrom->GetInventory();
	const auto it_item = inventory.find(a_item);
	if (it_item == inventory.end()) {
		logger::warn("Item {:x} not found in inventory {:x}", a_item ? a_item->GetFormID() : 0, moveFrom->GetFormID());
		return ref_handle;
	}

    const auto inv_data = it_item->second.second.get();
    if (const auto asd = inv_data ? inv_data->extraLists : nullptr; !asd || asd->empty()) {
        ref_handle = moveFrom->RemoveItem(a_item, 1, reason, nullptr, moveTo);
    } else {
        ref_handle = moveFrom->RemoveItem(a_item, 1, reason, asd->front(), moveTo);
    }
    return ref_handle;
}

template <typename T>
void Manager::UpdateFakeWV(T* fake_form, RE::TESObjectREFR* chest_linked, const float weight_ratio) {

    // assumes base container is already in the chest
    if (!chest_linked || !fake_form) return RaiseMngrErr("Failed to get chest.");
    const auto fake_formid = fake_form->GetFormID();
    auto real_container = FakeToRealContainer(fake_formid);
    // ReSharper disable once CppDependentTemplateWithoutTemplateKeyword
    fake_form->Copy(real_container->As<T>());  // NOLINT(clang-diagnostic-warning)
    if (renames.contains(fake_formid)) fake_form->fullName = renames.at(fake_form->GetFormID());

    FunctionsSkyrim::FormTraits<T>::SetWeight(fake_form, weight_ratio*chest_linked->GetWeightInContainer() + (1-weight_ratio) * real_container->GetWeight()); // dont change (1-weight_ratio)

    const auto chest_inventory = chest_linked->GetInventory();

    // get the ench costoverride of fake in player inventory
    int x_0 = real_container->GetGoldValue();
    const int target_value = Inventory::GetValueInContainer(chest_linked);

	int32_t extracost = 0;
    if (other_settings[Settings::otherstuffKeys[3]]) {
		if (auto temp_entry = chest_inventory.find(real_container); temp_entry != chest_inventory.end()) {
			extracost = Inventory::EntryHasXDataList(temp_entry->second.second.get()) ? xData::GetXDataCostOverride(temp_entry->second.second->extraLists->front()) : 0;
			x_0 = target_value - extracost;
		}
    }
    x_0 = std::max(x_0, 0);

    FunctionsSkyrim::FormTraits<T>::SetValue(fake_form, x_0);
        
 //   bool player_has_item = Inventory::HasItem(fake_form, player_ref);
	//RE::TESObjectREFR* container_location = GetContainerLocation(fake_form->GetFormID());
 //   if (!player_has_item && !container_location) return;

    const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_form->GetFormID());
    if (!fake_bound) return RaiseMngrErr("Fake bound is null");
    const int f_0 = fake_bound->GetGoldValue() + extracost; // player_has_item ? Inventory::GetItemValue(fake_bound, player_ref->GetInventory()) : container_location->GetGoldValue();
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
		logger::trace("Setting fake value to: {}", x_search);
		f_search = fake_bound->GetGoldValue() + extracost; //player_has_item ? Inventory::GetItemValue(fake_bound, player_ref->GetInventory()) : container_location->GetGoldValue();

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
            FunctionsSkyrim::FormTraits<T>::SetValue(fake_form, x_0);
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
