#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <spdlog/sinks/basic_file_sink.h>

namespace logger = SKSE::log;
using namespace std::literals;

constexpr uint32_t player_refid = 0x14;

using FormID = RE::FormID;
using RefID = RE::FormID;
using Count = RE::TESObjectREFR::Count;

constexpr float EPSILON = 1e-10f;