#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace takenc {
    class ApeTagWriter {
    public:
        void add_item(const std::string &key, const std::string &value);

        // Generates the full APEv2 tag (Header + Items + Footer) matching takc.exe
        std::vector<uint8_t> generate() const;

    private:
        std::map<std::string, std::string> items;
    };
} // namespace takenc
