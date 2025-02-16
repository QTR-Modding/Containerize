#include "FormReader.h"
#include <string>
#include <vector>
#include <regex>
#include <algorithm>
#include <cctype>


using namespace FormReader;


std::string FormReader::clean(const std::string& input)
{
    // Remove all non-alphanumeric characters
    std::string s = std::regex_replace(input, std::regex("[^a-zA-Z0-9]+"), "");
    
    // Convert to uppercase
    std::ranges::transform(s.begin(), s.end(), s.begin(), ::toupper);
    
    // Trim whitespace from both ends
    s.erase(s.begin(), std::ranges::find_if(s.begin(), s.end(), [](const unsigned char ch) {
        return !std::isspace(ch);
    }));
    s.erase(std::ranges::find_if(s.rbegin(), s.rend(), [](const unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
    
    return s;
}

Result FormReader::processInput(const std::string& input, const std::string& name)
{
    Result res;
    res.output = "";
    res.error = true;
    res.isMod = true;

    if (!input.empty())
    {
        std::string result = clean(input);
        std::string modName = name;
        
        // Trim modName
        modName.erase(modName.begin(), std::find_if(modName.begin(), modName.end(), [](const unsigned char ch) {
            return !std::isspace(ch);
        }));
        modName.erase(std::find_if(modName.rbegin(), modName.rend(), [](const unsigned char ch) {
            return !std::isspace(ch);
        }).base(), modName.end());

        if (result.length() != 8)
        {
            res.output += "input must be 8 characters long";
        }

        if (res.output.empty())
        {
            res.error = false;

            std::string firstTwoDigits = result.substr(0, 2);

            bool esl = false;
            bool mod = true;

            if (firstTwoDigits == "FE")
            {
                esl = true;
            }
            else if (std::find(masters.begin(), masters.end(), firstTwoDigits) != masters.end())
            {
                mod = false;
            }

            res.isMod = mod;

            if (mod)
            {
                if (esl)
                {
                    result = result.substr(5);
                }
                else
                {
                    result = result.substr(2);
                }
            }

            if (mod && modName.empty())
            {
                res.error = true;
                res.output = "Mod name required for non base game files";
            }
            else
            {
                size_t i = 0;
                while (i < result.length() && result[i] == '0')
                {
                    i++;
                }

                res.output = "0x" + result.substr(i) + (mod ? ("~" + name) : "");
            }
        }
    }

    return res;
}

FormID FormReader::GetFormIDFromString(const std::string& input)
{
    int form_id_;
    std::stringstream ss;
    ss << std::hex << input;
    ss >> form_id_;
    return form_id_;
}