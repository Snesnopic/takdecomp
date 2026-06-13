cmake_minimum_required(VERSION 3.12..4.3)

project(takdecomp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -O0")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Core Library (Decoder) ---
add_library(takdec_core STATIC
            src/core/tak_decoder.cpp
            src/core/tak_decoder_entropy.cpp
            src/core/tak_decoder_subframe.cpp
            src/core/tak_decoder_decorrelate.cpp
            src/core/tak_crc.cpp)

target_include_directories(takdec_core PUBLIC src/core/include)
target_link_libraries(takdec_core PRIVATE Threads::Threads)

# --- CLI Executable (Decoder) ---
add_executable(takdec src/cli/takdec.cpp)
target_link_libraries(takdec PRIVATE takdec_core)
target_include_directories(takdec PRIVATE src/cli)

# --- Core Library (Encoder) ---
add_library(takenc_core STATIC
            src/core/tak_encoder.cpp
            src/core/tak_encoder_wav.cpp
            src/core/tak_encoder_filter.cpp
            src/core/tak_encoder_subframe.cpp
            src/core/tak_encoder_bitstream.cpp
            src/core/tak_encoder_entropy.cpp
            src/core/tak_encoder_decorrelate.cpp
            src/core/tak_encoder_md5.cpp
            src/core/ape_tag.cpp
            src/core/tak_crc.cpp)

target_include_directories(takenc_core PUBLIC src/core/include)
target_link_libraries(takenc_core PRIVATE Threads::Threads)

# --- CLI Executable (Encoder) ---
add_executable(takenc src/cli/takenc.cpp)
target_link_libraries(takenc PRIVATE takenc_core)
target_include_directories(takenc PRIVATE src/cli)

include(GNUInstallDirs)
install(TARGETS takdec takenc DESTINATION ${CMAKE_INSTALL_BINDIR})
