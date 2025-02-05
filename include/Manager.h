#pragma once
#include "DynamicFormTracker.h"
#include <algorithm>
#include <shared_mutex>


class Manager : public SaveLoadData {
    // private variables
    const std::vector<std::string> buttons = {"Open", "Take", "More...", "Close"};
    const std::vector<std::string> buttons_more = {"Rename", "Uninstall", "Back", "Close"};
    bool uiextensions_is_present = false;
    RE::TESObjectREFR* player_ref = RE::PlayerCharacter::GetSingleton()->As<RE::TESObjectREFR>();
    //RE::EffectSetting* empty_mgeff = nullptr;
    
    //  maybe i dont need this by using uniqueID for new forms
    // runtime specific
    std::map<RefID,FormFormID> ChestToFakeContainer; // chest refid -> {real container formid (outerKey), fake container formid (innerKey)}
    RE::TESObjectREFR* current_container = nullptr;
    RE::TESObjectREFR* hidden_real_ref = nullptr;

    // unowned stuff
    const RefID unownedChestOGRefID = 0x000EA29A;
    const RefID unownedChestFormID = 0x000EA299;
    RE::TESObjectCELL* unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000EA28B);
    RE::TESObjectCONT* unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(unownedChestFormID);
    //RE::TESObjectCELL* unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000FE47B);  // cwquartermastercontainers
    //RE::TESObjectCONT* unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(0x000A0DB5); // playerhousechestnew
    const RE::NiPoint3 unownedChestPos = {1986.f, 1780.f, 6784.f};
    
    std::vector<FormID> external_favs; // runtime specific, FormIDs of fake containers if faved
    std::vector<RefID> handled_external_conts; // runtime specific to prevent unnecessary checks in HandleFakePlacement
    std::map<FormID,std::string> renames;  // runtime specific, custom names for fake containers
    std::pair<FormID, RefID> real_to_sendback = {0,0};  // pff


    void SendReal(FormID real_formid, RE::TESObjectREFR* chest);

    [[nodiscard]] RE::TESBoundObject* FakeToRealContainer(FormID fake);

    // Activates a container
    void Activate(RE::TESObjectREFR* a_objref);

    void ActivateChest(RE::TESObjectREFR* chest, const char* chest_name);

    [[nodiscard]] int GetChestValue(RE::TESObjectREFR* a_chest);

    // from container out in the world to linked chest
    [[nodiscard]] RE::TESObjectREFR* GetRealContainerChest(const RE::TESObjectREFR* real_container);

    [[nodiscard]] uint32_t GetNoChests() const;

    [[nodiscard]] std::vector<RefID> ConnectedChests(RefID chestRef);

    [[nodiscard]] bool IsUnownedChest(RefID refid) const;

    [[nodiscard]] RE::TESObjectREFR* MakeChest(RE::NiPoint3 Pos3 = {0.0f, 0.0f, 0.0f}) const;

    [[nodiscard]] RE::TESObjectREFR* AddChest(uint32_t chest_no) const;

    [[nodiscard]] RE::TESObjectREFR* FindNotMatchedChest() const;

    [[nodiscard]] RefID GetFakeContainerChest(FormID fake_id);

    std::vector<FormID> RemoveAllItemsFromChest(RE::TESObjectREFR* chest, RE::TESObjectREFR* move2ref = nullptr);

    std::vector<FormID> DeRegisterChest(RefID chest_ref);

    // OK. from real container formid to linked source
    [[nodiscard]] Source* GetContainerSource(FormID container_formid);

    // returns true only if the item is in the inventory with positive count. removes the item if it is in the inventory with 0 count
    [[nodiscard]] bool HasItemPlusCleanUp(RE::TESBoundObject* item, RE::TESObjectREFR* item_owner);

    // removes only one unit of the item
    RE::ObjectRefHandle RemoveItem(RE::TESObjectREFR* moveFrom, RE::TESObjectREFR* moveTo, FormID item_id,
                                          RE::ITEM_REMOVE_REASON reason);

    [[nodiscard]] bool PickUpItem(RE::TESObjectREFR* item, unsigned int max_try = 3);

    // Removes the object from the world and adds it to an inventory
    [[nodiscard]] static bool MoveObject(RE::TESObjectREFR* ref, RE::TESObjectREFR* move2container, bool owned = true);

    template <typename T>
    void UpdateFakeWV(T* fake_form, RE::TESObjectREFR* chest_linked, float weight_ratio);

    // Updates weight and value of fake container and uses Copy and applies renaming
    void UpdateFakeWV(RE::TESBoundObject* fake_form, RE::TESObjectREFR* chest_linked, float weight_ratio);

    [[nodiscard]] bool UpdateExtrasInInventory(RE::TESObjectREFR* from_inv, FormID from_item_formid,
                                               RE::TESObjectREFR* to_inv, FormID to_item_formid);

    void HandleFormDelete_(RefID chest_refid);


    std::shared_mutex sharedMutex_;
    std::vector<Source> sources;


    void RaiseMngrErr(const std::string& err_msg_ = "Error");

    void InitFailed();

    void Init();


    template <typename T>
    FormID CreateFakeContainer(T* realcontainer, RefID connected_chest, RE::ExtraDataList*);

    // Creates new form for fake container // pre 0.7.1: and adds it to unownedChestOG
    FormID CreateFakeContainer(RE::TESBoundObject* container, RefID connected_chest, RE::ExtraDataList* el);

    


    // external refid is in one of the source data. unownedchests are allowed
    [[nodiscard]] bool ExternalContainerIsRegistered(RefID external_container_id);

    // for the cases when real container is in its chest and fake container is in some other inventory (player,unownedchest,external_container)
    // DOES NOT UPDATE THE SOURCE DATA and CHESTTOFAKECONTAINER !!!
    void qTRICK_(SourceDataKey chest_ref, SourceDataVal cont_ref,bool fake_nonexistent = false);

    // places fakes according to loaded data to player or unowned chests
    void Something2(const RefID chest_ref, std::vector<RefID>& ha);

    void FakePlacement(RefID saved_ref, RefID chest_ref, RE::TESObjectREFR* external_cont = nullptr);

    void RemoveCarryWeightBoost(FormID item_formid, RE::TESObjectREFR* inventory_owner);

    bool HandleRegistration(RE::TESObjectREFR* a_container);

    void MsgBoxCallback(int result);

    void MsgBoxCallbackMore(const int result);

    void PromptInterface();

    template <typename T>
    static void Rename(const std::string& new_name, T item) {
        logger::trace("Rename");
        if (!item) logger::warn("Item not found");
        else item->fullName = new_name;
    }


public:

    [[nodiscard]] RefID GetRealContainerChestID(RefID real_refid) const;
    RE::TESBoundObject* GetFakeBound(RefID chest_id) const;
    void HandlePickup(const RE::TESObjectREFR* picked_up_by, RE::TESObjectREFR * a_object);

    explicit Manager(const std::vector<Source>& data) : sources(data) { Init(); }

    static Manager* GetSingleton(const std::vector<Source>& data) {
        static Manager singleton(data);
        return &singleton;
    }

    const char* GetType() override { return "Manager"; }

	std::atomic<bool> isUninstalled = false;
	std::atomic<bool> listen_menu_close = false;
    std::atomic<bool> listen_activate = true;
    std::atomic<bool> listen_container_change = true;


    void OnActivateContainer(RE::TESObjectREFR* a_container);

    // places fake objects in external containers after load game
    void HandleFakePlacement(RE::TESObjectREFR* external_cont);

    [[nodiscard]] bool IsFakeContainer(FormID formid);

    // Checks if realcontainer_formid is in the sources
    [[nodiscard]] bool IsRealContainer(FormID formid) const;

    // Checks if ref has formid in the sources
    [[nodiscard]] bool IsRealContainer(const RE::TESObjectREFR* ref) const;

    void RenameContainer(const std::string& new_name);

    // reverts inside the same inventory
    static void RevertEquip(FormID fakeid);

    // reverts by sending it back to the initial inventory
    void RevertEquip(FormID fakeid, RefID external_container_id);

    void HandleContainerMenuExit();

    void ActivateContainer(FormID fakeid, bool hide_real = false);

    void UnHideReal(FormID fakeid);

    [[nodiscard]] bool IsARegistry(RefID registry) const;

    // if the src with this formid has some data, then we say it has registry
    [[nodiscard]] bool RealContainerHasRegistry(FormID realcontainer_formid) const;

    // hopefully this works.
    void DropTake(FormID realcontainer_formid, RefID refid);

    // external container can be found in the values of src.data
    [[nodiscard]] bool ExternalContainerIsRegistered(FormID fake_container_formid,
                                                     RefID external_container_id);

    void UnLinkExternalContainer(FormID fake_container_formid, RefID externalcontainer);

    [[nodiscard]] bool SwapDroppedFakeContainer(RE::TESObjectREFR* ref_fake);

    void HandleCraftingExit();

    void HandleConsume(FormID fake_formid);

    void HandleSell(FormID fake_container, RefID sell_refid);

    void HandleFormDelete(RefID refid);

    [[nodiscard]] static bool IsCONT(RefID refid);

    // checks if the refid is in the ChestToFakeContainer, i.e. if it is an unownedchest
    [[nodiscard]] bool IsChest(const RefID chest_refid) const { return ChestToFakeContainer.contains(chest_refid); }

    // Register an external container (technically could be another unownedchest of another of our containers) to the source data so that chestrefid of currentcontainer -> external container
    void LinkExternalContainer(FormID fakecontainer, RefID externalcontainer_refid);

    void InspectItemTransfer(RefID chest_refid, FormID item_id);

    void Reset();

    void Print();

    void SendData();

    void ReceiveData();

	const std::vector<Source>& GetSources() const { return sources; }

    void Uninstall();

};



template <typename T>
void Manager::UpdateFakeWV(T* fake_form, RE::TESObjectREFR* chest_linked, float weight_ratio) {
    logger::trace("UpdateFakeWV");
        
    // assumes base container is already in the chest
    if (!chest_linked || !fake_form) return RaiseMngrErr("Failed to get chest.");
    const auto fake_formid = fake_form->GetFormID();
    auto real_container = FakeToRealContainer(fake_formid);
    logger::trace("Copying from real container to fake container. Real container: {}, Fake container: {}",
                  real_container->GetFormID(), fake_formid);
    fake_form->Copy(real_container->As<T>());
    logger::trace("Copied from real container to fake container");
    logger::trace("if it was renamed, rename it back");
    if (!renames.empty() && renames.count(fake_formid)) fake_form->fullName = renames[fake_form->GetFormID()];

    if (weight_ratio > 0.f) FunctionsSkyrim::FormTraits<T>::SetWeight(fake_form, weight_ratio*chest_linked->GetWeightInContainer() + (1-weight_ratio)*real_container->GetWeight());

    auto chest_inventory = chest_linked->GetInventory();
#ifndef NDEBUG
    for (auto& [key, value] : chest_inventory) {
        logger::trace("Item: {}, Count: {}", key->GetName(), value.first);
    }
#endif

    // get the ench costoverride of fake in player inventory

    int x_0 = real_container->GetGoldValue();
    const int target_value = Inventory::GetValueInContainer(chest_linked);

    if (other_settings[Settings::otherstuffKeys[3]]) {
        logger::trace("VALUE BEFORE {}", x_0);
        auto temp_entry = chest_inventory.find(real_container);
        const auto extracost = Inventory::EntryHasXData(temp_entry->second.second.get()) ? xData::GetXDataCostOverride(temp_entry->second.second->extraLists->front()) : 0;
        logger::trace("extracost {}", extracost);
        x_0 = target_value - extracost;
        logger::trace("VALUE AFTER {}", x_0);
    }
    x_0 = std::max(x_0, 0);

    logger::trace("Setting weight and value for fake form");
    FunctionsSkyrim::FormTraits<T>::SetValue(fake_form, x_0);
    logger::trace("ACTUAL VALUE {}", FunctionsSkyrim::FormTraits<T>::GetValue(fake_form));
        
    if (!Inventory::HasItem(fake_form, player_ref) || x_0 == 0) return;

    const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_form->GetFormID());
    if (!fake_bound) return RaiseMngrErr("Fake bound is null");
    const int f_0 = Inventory::GetItemValue(fake_bound, player_ref->GetInventory());
    int f_search = f_0;
    logger::trace("Value in inventory: {}, Target value: {}", f_0, target_value);
    //if (f_0 <= target_value) return;

    logger::trace("Player has the fake form, try to correct the value");
    // do binary search to find the correct value up to a tolerance
    constexpr float tolerance = 0.01f; // 1%
    const float tolerance_val = std::max(2.0f, std::floor(tolerance * target_value) + 1);  // at least 2
    constexpr int max_iter = 1000;
    int curr_iter = max_iter;

    int lower_bound = 0;
    int upper_bound = x_0;
    int x_search = (lower_bound + upper_bound) / 2;

    logger::trace("Value in inventory: {}", f_search);
    logger::trace("x_0: {}", x_0);

    while (std::abs(f_search - target_value) > tolerance_val && curr_iter > 0) {
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

    logger::trace("iter: {}", max_iter - curr_iter);

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
    logger::trace("CreateFakeContainer");
    //new_form = realcontainer->CreateDuplicateForm(true, (void*)new_form)->As<T>();
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
    //unownedChestOG->AddObjectToContainer(new_form, extralist, 1, nullptr); // pre 0.7.1

    return new_form_id;
}