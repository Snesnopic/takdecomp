#include "tak_encoder/ape_tag.hpp"
#include <cstring>

namespace takenc {
    void ApeTagWriter::add_item(const std::string &key, const std::string &value) {
        items[key] = value;
    }

    static void write_le32(std::vector<uint8_t> &out, const uint32_t val) {
        out.push_back(val & 0xFF);
        out.push_back((val >> 8) & 0xFF);
        out.push_back((val >> 16) & 0xFF);
        out.push_back((val >> 24) & 0xFF);
    }

    std::vector<uint8_t> ApeTagWriter::generate() const {
        if (items.empty()) return {};

        std::vector<uint8_t> out;
        std::vector<uint8_t> items_data;

        for (const auto &pair: items) {
            const uint32_t val_size = pair.second.size();
            constexpr uint32_t flags = 0; // UTF-8 string, Read-Write

            // Write Value Size
            write_le32(items_data, val_size);
            // Write Flags
            write_le32(items_data, flags);
            // Write Key (ASCII + \0)
            for (const char c: pair.first) items_data.push_back(c);
            items_data.push_back(0);
            // Write Value
            for (const char c: pair.second) items_data.push_back(c);
        }

        const uint32_t tag_size = items_data.size() + 32; // size of items + footer
        const uint32_t item_count = items.size();

        // Header
        constexpr auto magic = "APETAGEX";
        out.reserve(8);
        for (int i = 0; i < 8; i++) out.push_back(magic[i]);
        write_le32(out, 2000); // Version
        write_le32(out, tag_size);
        write_le32(out, item_count);
        write_le32(out, 0xA0000000); // Contains Header | Is Header
        for (int i = 0; i < 8; i++) out.push_back(0);

        // Items
        out.insert(out.end(), items_data.begin(), items_data.end());

        // Footer
        for (int i = 0; i < 8; i++) out.push_back(magic[i]);
        write_le32(out, 2000); // Version
        write_le32(out, tag_size);
        write_le32(out, item_count);
        write_le32(out, 0x80000000); // Contains Header | Is NOT Header
        for (int i = 0; i < 8; i++) out.push_back(0);

        return out;
    }
} // namespace takenc
