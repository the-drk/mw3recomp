cmake_minimum_required(VERSION 3.20)
project(mw3_public_dependencies C CXX)

# No title configuration, extracted assets, or generated PPC headers are used.
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../third_party/ffmpeg-xenia"
                 "${CMAKE_CURRENT_BINARY_DIR}/ffmpeg-xenia")
