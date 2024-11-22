# Copyright MediaZ Teknoloji A.S. All Rights Reserved.
cmake_minimum_required(VERSION 3.24.2)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_STANDARD 20)

if (NOT WITH_NODOS_WORKSPACE)
    message(FATAL_ERROR "This repo currently does not support builds without Nodos workspace. "
    "Place this repo under nodos-workspace/Module folder and run cmake -S ./Toolchain/CMake -B Build from workspace root.")
endif()

# Nodos SDK
nos_find_sdk("1.3.0" NOS_PLUGIN_SDK_TARGET NOS_SUBSYSTEM_SDK_TARGET NOS_SDK_DIR)
set(FLATC_EXECUTABLE ${NOS_SDK_DIR}/bin/flatc${CMAKE_EXECUTABLE_SUFFIX})

# DeckLink SDK
# ------------
set(DECKLINK_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/External/DeckLinkSDK/")

if (WIN32)
     # Add DeckLink API
    set(PLATFORM_SOURCES
            ${CMAKE_CURRENT_BINARY_DIR}/DeckLinkAPI_i.c
            ${CMAKE_CURRENT_BINARY_DIR}/DeckLinkAPI.h)

    set(DECKLINK_SDK_INCLUDE_DIR ${DECKLINK_SDK_DIR}/Win/include)

    # Link COM support
    list(APPEND PLATFORM_LIBRARIES comsuppw.lib)

    # Compile DeckLink API IDL to build directory
    set(MIDL_OUTPUT_SOURCE "${CMAKE_CURRENT_BINARY_DIR}\\DeckLinkAPI_i.c")
    set(MIDL_OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}\\DeckLinkAPI.h")
    set(MIDL_OUTPUT_TLB "${CMAKE_CURRENT_BINARY_DIR}\\DeckLinkAPI.tlb")
    set(DECKLINK_API_INTEROP "${CMAKE_CURRENT_BINARY_DIR}\\Interop.DeckLinkAPI.dll")
    set(DECKLINK_API_IDL "${DECKLINK_SDK_INCLUDE_DIR}\\DeckLinkAPI.idl")
    set(DECKLINK_SDK_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR})

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)")
        set(MIDL_MACHINE win32)
    else()
        set(MIDL_MACHINE amd64)
    endif()

    set(MIDL_OUTPUT
            ${MIDL_OUTPUT_SOURCE}
            ${MIDL_OUTPUT_HEADER}
            ${MIDL_OUTPUT_TLB})

    # Compile the DeckLinkAPI.idl file (to generate DeckLinkAPI.h and DeckLinkAPI_i.c)
    add_custom_command(
            OUTPUT ${MIDL_OUTPUT}
            DEPENDS ${DECKLINK_API_IDL}
            COMMAND midl /env ${MIDL_MACHINE} /header ${MIDL_OUTPUT_HEADER} ${DECKLINK_API_IDL} /tlb ${MIDL_OUTPUT_TLB}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            )

    set_source_files_properties(${MIDL_OUTPUT} PROPERTIES GENERATED TRUE)

    add_custom_target(MIDL_Target DEPENDS ${MIDL_OUTPUT})
endif()


# nos.sys.vulkan
nos_get_module("nos.sys.vulkan" "5.8" NOS_SYS_VULKAN_TARGET_5_8)

# nos.mediaio
nos_get_module("nos.mediaio" "2.0" NOS_MEDIAIO_TARGET)
nos_find_module_path("nos.mediaio" "2.0" NOS_MEDIAIO_PATH)
nos_generate_flatbuffers("${NOS_MEDIAIO_PATH}/Config" "${CMAKE_CURRENT_SOURCE_DIR}/Source/Generated" "cpp" "${NOS_MEDIAIO_PATH};${NOS_SDK_DIR}/types" generated_nosDeckLink_dep_nosMediaIO)

# nos.utilities
nos_get_module("nos.utilities" "2.9" NOS_UTILITIES_TARGET)
nos_find_module_path("nos.utilities" "2.9" NOS_UTILITIES_PATH)
nos_generate_flatbuffers("${NOS_UTILITIES_PATH}/Config" "${CMAKE_CURRENT_SOURCE_DIR}/Source/Generated" "cpp" "${NOS_UTILITIES_PATH};${NOS_SDK_DIR}/types" generated_nosDeckLink_dep_nosUtilities)

list(APPEND DEPENDENCIES generated_nosDeckLink_dep_nosMediaIO generated_nosDeckLink_dep_nosUtilities ${NOS_SYS_VULKAN_TARGET_5_8} ${NOS_PLUGIN_SDK_TARGET})
list(APPEND INCLUDE_FOLDERS
    ${EXTERNAL_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_BINARY_DIR})

nos_add_plugin("nosDeckLink" "${DEPENDENCIES}" "${INCLUDE_FOLDERS}")

target_sources(nosDeckLink PRIVATE ${PLATFORM_SOURCES})

if (WIN32)
    add_dependencies(nosDeckLink MIDL_Target)
endif()

# Project generation
nos_group_targets("nosDeckLink" "NOS Plugins")