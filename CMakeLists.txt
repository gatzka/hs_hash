cmake_minimum_required(VERSION 3.14)
project(hs_hash VERSION 0.0.1 LANGUAGES C)

include(GNUInstallDirs)

add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_compile_features(${PROJECT_NAME} INTERFACE c_std_99)
target_include_directories(${PROJECT_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Only generate install rules if hs_hash is the top level project, not if a
# parent project pulls it in via add_subdirectory() or FetchContent. A parent
# project can still enable them by setting HS_HASH_INSTALL.
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(HS_HASH_IS_TOP_LEVEL ON)
else()
    set(HS_HASH_IS_TOP_LEVEL OFF)
endif()
option(HS_HASH_INSTALL "Generate install rules for hs_hash" ${HS_HASH_IS_TOP_LEVEL})

if(HS_HASH_INSTALL)
    # The package is header only, so its CMake files are architecture
    # independent and go to share/ instead of lib/.
    set(HS_HASH_CMAKE_DIR ${CMAKE_INSTALL_DATADIR}/cmake/${PROJECT_NAME})

    install(FILES hs_hash/hs_hash.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
    )

    install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME})
    install(EXPORT ${PROJECT_NAME}
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION ${HS_HASH_CMAKE_DIR}
        FILE ${PROJECT_NAME}Config.cmake
    )

    include(CMakePackageConfigHelpers)
    write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION ${HS_HASH_CMAKE_DIR}
    )
endif()
