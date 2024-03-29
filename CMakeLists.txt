# This file is part of libflowdrop.
#
# For license and copyright information please follow this link:
# https://github.com/noseam-env/libflowdrop/blob/master/LEGAL

cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

cmake_policy(SET CMP0077 NEW)

project(libflowdrop LANGUAGES C CXX)

option(LIBFLOWDROP_BUILD_STATIC "BUILD STATIC LIBRARIES" ON)
option(LIBFLOWDROP_BUILD_SHARED "BUILD SHARED LIBRARIES" ON)
option(ENABLE_KNOT_DNSSD "ENABLE KNOT DNS-SD" ON)

set(CMAKE_CXX_STANDARD 17)

set(LIBFLOWDROP_SOURCES
        src/knotport/knotport.c
        src/knotport/knotport.h
        src/os/file_info.c
        src/os/file_info.h
        src/core.cpp
        src/core.h
        src/discovery.cpp
        src/discovery.hpp
        src/logger.cpp
        src/logger.h
        src/send_request.cpp
        src/server.cpp
        src/specification.h)

set(LIBFLOWDROP_PRIVATE_LIBS
        nlohmann_json::nlohmann_json
        virtualtfa_static
        hv_static
        libcurl)

if(ANDROID)
    list(APPEND LIBFLOWDROP_PRIVATE_LIBS log)
    set(LIBFLOWDROP_DEFS ${LIBFLOWDROP_DEFS} IPV6_NOT_SUPPORTED)
endif ()

set(LIBFLOWDROP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(LIBFLOWDROP_TARGET_INCLUDE ${LIBHV_HEADERS})

if (ENABLE_KNOT_DNSSD)
    add_subdirectory(ThirdParty/libknotdnssd)
    list(APPEND LIBFLOWDROP_PRIVATE_LIBS knotdnssd_static)
endif ()

set(VIRTUALTFA_BUILD_SHARED OFF)

add_subdirectory(ThirdParty/curl)
add_subdirectory(ThirdParty/libhv)
add_subdirectory(ThirdParty/libvirtualtfa)
add_subdirectory(ThirdParty/nlohmann_json)

if (WIN32)
    option(BUILD_FOR_MT "build for /MT" OFF)
    if (BUILD_FOR_MT)
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    endif ()

    list(APPEND LIBFLOWDROP_PRIVATE_LIBS ws2_32 ntdll)
endif ()

if (UNIX)
    set_property(TARGET libcurl PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET hv_static PROPERTY POSITION_INDEPENDENT_CODE ON)
endif ()

if (LIBFLOWDROP_BUILD_SHARED)
    add_library(libflowdrop_shared SHARED ${LIBFLOWDROP_SOURCES})
    target_compile_definitions(libflowdrop_shared PRIVATE ${LIBFLOWDROP_DEFS})
    target_include_directories(libflowdrop_shared PUBLIC ${LIBFLOWDROP_INCLUDE_DIR})
    target_include_directories(libflowdrop_shared PRIVATE ${LIBFLOWDROP_TARGET_INCLUDE})
    target_link_libraries(libflowdrop_shared PRIVATE ${LIBFLOWDROP_PRIVATE_LIBS})
endif ()
if (LIBFLOWDROP_BUILD_STATIC)
    add_library(libflowdrop_static STATIC ${LIBFLOWDROP_SOURCES})
    target_compile_definitions(libflowdrop_static PRIVATE ${LIBFLOWDROP_DEFS})
    target_include_directories(libflowdrop_static PUBLIC ${LIBFLOWDROP_INCLUDE_DIR})
    target_include_directories(libflowdrop_static PRIVATE ${LIBFLOWDROP_TARGET_INCLUDE})
    target_link_libraries(libflowdrop_static PRIVATE ${LIBFLOWDROP_PRIVATE_LIBS})
endif ()
