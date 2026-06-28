#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <istream>

namespace takenc {
    class ApeTagWriter {
    public:
        void add_item(const std::string &key, const std::string &value);

        // Generates the full APEv2 tag (Header + Items + Footer) matching takc.exe
        [[nodiscard]] std::vector<uint8_t> generate() const;

    private:
        std::map<std::string, std::string> items;
    };

    struct ApeTagItem {
        std::string key;
        std::vector<uint8_t> value;
    };

    class ApeTag {
    public:
        std::vector<ApeTagItem> items;
    };

    class ApeTagReader {
    public:
        static ApeTag read_from_stream(std::istream& is);
    };
} // namespace takenc
