cmake_minimum_required(VERSION 3.16)
project(DJIReframeLinux LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(OFX_ROOT "${CMAKE_SOURCE_DIR}/vendor/openfx")
if(NOT EXISTS "${OFX_ROOT}/include/ofxImageEffect.h")
    message(FATAL_ERROR
        "OpenFX SDK not found at ${OFX_ROOT}.\n"
        "Run: git clone --depth 1 https://github.com/AcademySoftwareFoundation/openfx.git vendor/openfx")
endif()

file(GLOB OFX_SUPPORT_SRC "${OFX_ROOT}/Support/Library/*.cpp")

add_library(DJIReframe MODULE
    src/DJIReframe.cpp
    ${OFX_SUPPORT_SRC}
)

target_include_directories(DJIReframe PRIVATE
    "${OFX_ROOT}/include"
    "${OFX_ROOT}/Support/include"
    src
)

# OFX plugins are .ofx, not lib*.so
set_target_properties(DJIReframe PROPERTIES
    PREFIX ""
    SUFFIX ".ofx"
    OUTPUT_NAME "DJIReframe"
)

target_compile_options(DJIReframe PRIVATE -O3 -ffast-math -Wall -Wno-unused-parameter)
target_link_options(DJIReframe PRIVATE -Wl,--no-undefined)
find_package(Threads REQUIRED)
target_link_libraries(DJIReframe PRIVATE Threads::Threads)

# ---------------------------------------------------------------- bundle
# OFX expects DJIReframe.ofx.bundle/Contents/Linux-x86-64/DJIReframe.ofx
set(BUNDLE_DIR "${CMAKE_BINARY_DIR}/DJIReframe.ofx.bundle")
set(BUNDLE_BIN "${BUNDLE_DIR}/Contents/Linux-x86-64")

add_custom_command(TARGET DJIReframe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_BIN}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_DIR}/Contents/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:DJIReframe>" "${BUNDLE_BIN}/"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/packaging/Info.plist" "${BUNDLE_DIR}/Contents/Info.plist"
    COMMENT "Assembling DJIReframe.ofx.bundle"
)

# "sudo cmake --install build" -> /usr/OFX/Plugins
if(NOT DEFINED OFX_PLUGIN_DIR)
    set(OFX_PLUGIN_DIR "/usr/OFX/Plugins")
endif()
install(DIRECTORY "${BUNDLE_DIR}" DESTINATION "${OFX_PLUGIN_DIR}" USE_SOURCE_PERMISSIONS)
