#include "Serialization.h"


std::vector<std::pair<int, bool>> encodeString(const std::string& inputString)
{
    std::vector<std::pair<int, bool>> encodedValues;
    try {
        for (int i = 0; i < 100 && inputString[i] != '\0'; i++) {
            char ch = inputString[i];
            if (std::isprint(ch) && (std::isalnum(ch) || std::isspace(ch) || std::ispunct(ch)) && ch >= 0 &&
                ch <= 255) {
                encodedValues.push_back(std::make_pair(static_cast<int>(ch), std::isupper(ch)));
            }
        }
    } catch (const std::exception& e) {
        logger::error("Error encoding string: {}", e.what());
        return encodeString("ERROR");
    }
    return encodedValues;
}

std::string decodeString(const std::vector<std::pair<int, bool>>& encodedValues)
{
    std::string decodedString;
    for (const auto& pair : encodedValues) {
        char ch = static_cast<char>(pair.first);
        if (std::isalnum(ch) || std::isspace(ch) || std::ispunct(ch)) {
            if (pair.second) {
                decodedString += ch;
            } else {
                decodedString += static_cast<char>(std::tolower(ch));
            }
        }
    }
    return decodedString;
}

bool read_string(SKSE::SerializationInterface* a_intfc, std::string& a_str)
{
    std::vector<std::pair<int, bool>> encodedStr;
	std::size_t size;
    if (!a_intfc->ReadRecordData(size)) {
        return false;
    }
    for (std::size_t i = 0; i < size; i++) {
        std::pair<int, bool> temp_pair;
        if (!a_intfc->ReadRecordData(temp_pair)) {
			return false;
		}
        encodedStr.push_back(temp_pair);
	}
    a_str = decodeString(encodedStr);
	return true;
}

bool write_string(SKSE::SerializationInterface* a_intfc, const std::string& a_str)
{
    const auto encodedStr = encodeString(a_str);
    // i first need the size to know no of iterations
    const auto size = encodedStr.size();
    if (!a_intfc->WriteRecordData(size)) {
		return false;
	}
    for (const auto& temp_pair : encodedStr) {
        if (!a_intfc->WriteRecordData(temp_pair)) {
            return false;
        }
    }
    return true;
};