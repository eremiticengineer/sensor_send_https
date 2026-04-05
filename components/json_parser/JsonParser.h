#pragma once
#include "cJSON.h"
#include <string>

class JsonParser {
public:
    JsonParser() = default;
    ~JsonParser() = default;

    // Parse JSON string
    bool parse(const std::string& jsonStr);

    // Getters for fields
    std::string getName() const { return name; }
    std::string getVersion() const { return version; }
    std::string getDate() const { return date; }

private:
    std::string name;
    std::string version;
    std::string date;
};
