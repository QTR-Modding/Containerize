#pragma once
#include <ranges>

namespace FormReader {

    // Global masters list
    const std::vector<std::string> masters = {"00", "01", "02", "03", "04"};

    // Function to clean the input string
    std::string clean(const std::string& input);

    // Struct to hold the result
    struct Result
    {
        std::string output;
        bool error;
        bool isMod;
    };

    // Function to process the input and produce the output
    Result processInput(const std::string& input, const std::string& name);


    inline std::vector<std::string> split(const std::string& a_str, std::string_view a_delimiter)
    {
        auto range = a_str | std::ranges::views::split(a_delimiter) | 
                          std::ranges::views::transform([](auto&& r) { return std::string_view(r); });
        return { range.begin(), range.end() };
    }


    inline RE::FormID GetForm(const char* fileName, const RE::FormID localId) {
        const auto dataHandler = RE::TESDataHandler::GetSingleton();
        const auto formId = dataHandler->LookupFormID(localId, fileName);
        return formId;
    }

	FormID GetFormIDFromString(const std::string& input);
}