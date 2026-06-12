#include "tak_encoder/encoder.hpp"
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input.wav> <output.tak>\n";
        return 1;
    }
    
    try {
        takenc::Encoder::encode_file(argv[1], argv[2]);
        std::cout << "Encoded successfully to " << argv[2] << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
