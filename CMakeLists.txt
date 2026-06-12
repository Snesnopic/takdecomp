cmake_minimum_required(VERSION 3.12..4.3)

project(takdecomp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Core Library ---
add_library(takdecomp_core STATIC
            src/core/tak_decoder.cpp
            src/core/tak_decoder_entropy.cpp
            src/core/tak_decoder_subframe.cpp
            src/core/tak_decoder_decorrelate.cpp
            src/core/tak_crc.cpp)

target_include_directories(takdecomp_core PUBLIC src/core/include)

# --- CLI Executable (Decoder) ---
add_executable(takdecomp src/cli/main.cpp)
target_link_libraries(takdecomp PRIVATE takdecomp_core)
target_include_directories(takdecomp PRIVATE src/cli)

# --- Encoder Core Library ---
add_library(takenc_core STATIC
            src/core/tak_encoder.cpp
            src/core/tak_encoder_wav.cpp
            src/core/tak_encoder_filter.cpp
            src/core/tak_encoder_subframe.cpp
            src/core/tak_encoder_bitstream.cpp
            src/core/tak_encoder_entropy.cpp
            src/core/tak_encoder_decorrelate.cpp
            src/core/tak_encoder_md5.cpp
            src/core/tak_crc.cpp)

target_include_directories(takenc_core PUBLIC src/core/include)

# --- CLI Executable (Encoder) ---
add_executable(takenc src/cli/takenc.cpp)
target_link_libraries(takenc PRIVATE takenc_core)
target_include_directories(takenc PRIVATE src/cli)
