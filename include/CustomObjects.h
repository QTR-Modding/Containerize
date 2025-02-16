#pragma once
#include "Utils.h"

struct FormRefID {
    FormID outerKey;  // real formid
    RefID innerKey;   // refid of unowned

    FormRefID() : outerKey(0), innerKey(0) {}
    FormRefID(const FormID value1, const RefID value2) : outerKey(value1), innerKey(value2) {}

    bool operator<(const FormRefID& other) const;
};

struct FormIDX {
    FormID id;         // fake formid
    bool equipped;     // is equippedB
    bool favorited;    // is favorited
    std::string name;  //(new) name
    FormIDX() : id(0), equipped(false), favorited(false), name("") {}
    FormIDX(FormID id, bool value1, bool value2, const std::string& value3);
};

struct FormRefIDX {
    FormIDX outerKey;  // see above
    RefID innerKey;    // refid of unowned/realoutintheworld/externalcont

    FormRefIDX() : innerKey(0) {}
    FormRefIDX(FormIDX value1, RefID value2) : outerKey(value1), innerKey(value2) {}

    bool operator<(const FormRefIDX& other) const;
};

struct FormFormID {  // used by ChestToFakeContainer
    FormID outerKey;
    FormID innerKey;
    bool operator<(const FormFormID& other) const;
};

using SourceDataKey = RefID;  // Chest Ref ID
using SourceDataVal = RefID;  // Container Ref ID if it exists otherwise Chest Ref ID

struct Source {

    using SourceData = std::map<RefID, RefID>;  // Chest-Container Reference ID Pairs

	float weight_ratio; // 1 - cloud storage, i.e. how much of the weight actually counts in percentage
    std::map<FormID,Count> initial_items;
    float capacity;
    std::uint32_t formid;
    std::string editorid;
    SourceData data;

    Source(std::uint32_t id, std::string id_str, float capacity, float cs);

    [[nodiscard]] std::string_view GetName() const;

    [[nodiscard]] RE::TESBoundObject* GetBoundObject() const;

    void AddInitialItem(FormID form_id, Count count);

    [[nodiscard]] bool IsHealthy() const;

};