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
    ApeTag ApeTagReader::read_from_stream(std::istream& is) {
        ApeTag tag;
        is.clear();
        is.seekg(0, std::ios::end);
        std::streampos file_size = is.tellg();
        if (file_size < 32) return tag;
        
        is.seekg(-32, std::ios::end);
        uint8_t footer[32];
        is.read(reinterpret_cast<char*>(footer), 32);
        
        if (std::memcmp(footer, "APETAGEX", 8) != 0) return tag;
        
        uint32_t size = footer[12] | (footer[13] << 8) | (footer[14] << 16) | (footer[15] << 24);
        uint32_t item_count = footer[16] | (footer[17] << 8) | (footer[18] << 16) | (footer[19] << 24);
        
        if (size < 32 || size > file_size) return tag;
        
        uint32_t payload_size = size - 32;
        if (payload_size == 0 || item_count == 0) return tag;
        
        is.seekg(-(std::streamoff)size, std::ios::end);
        std::vector<uint8_t> payload(payload_size);
        is.read(reinterpret_cast<char*>(payload.data()), payload_size);
        
        size_t pos = 0;
        for (uint32_t i = 0; i < item_count && pos + 8 < payload_size; i++) {
            uint32_t item_len = payload[pos] | (payload[pos+1] << 8) | (payload[pos+2] << 16) | (payload[pos+3] << 24);
            pos += 8; // skip len + flags
            
            std::string key;
            while (pos < payload_size && payload[pos] != 0) {
                key.push_back(static_cast<char>(payload[pos]));
                pos++;
            }
            pos++; // skip null
            
            if (pos + item_len <= payload_size) {
                ApeTagItem item;
                item.key = key;
                item.value.assign(payload.begin() + pos, payload.begin() + pos + item_len);
                tag.items.push_back(item);
                pos += item_len;
            } else {
                break;
            }
        }
        
        return tag;
    }
} // namespace takenc
