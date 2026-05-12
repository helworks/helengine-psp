cmake_minimum_required(VERSION 3.11)

project(helengine_psp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

option(HELENGINE_PSP_ISOLATED_BOOT "Build the PSP player in isolated blank-frame boot mode." ON)
option(HELENGINE_PSP_ENABLE_RUNTIME_STARTUP "Build the PSP player with checkpointed runtime startup enabled." OFF)

if("${HELENGINE_CORE_CPP_ROOT}" STREQUAL "")
    message(FATAL_ERROR "HELENGINE_CORE_CPP_ROOT must point at the generated helengine.core C++ output folder.")
endif()

if(NOT EXISTS "${HELENGINE_CORE_CPP_ROOT}/helcpp_config.hpp")
    message(FATAL_ERROR "HELENGINE_CORE_CPP_ROOT does not contain helcpp_config.hpp. Regenerate helengine.core C++ output first.")
endif()

if(NOT EXISTS "${HELENGINE_CORE_CPP_ROOT}/helengine_core_amalgamated.cpp")
    message(FATAL_ERROR "HELENGINE_CORE_CPP_ROOT does not contain helengine_core_amalgamated.cpp. Regenerate helengine.core C++ output first.")
endif()

if(NOT EXISTS "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_startup_manifest.cpp")
    message(FATAL_ERROR "HELENGINE_CORE_CPP_ROOT does not contain runtime/runtime_startup_manifest.cpp. Regenerate helengine.core C++ output first.")
endif()

if(NOT EXISTS "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_scene_catalog_manifest.cpp")
    message(FATAL_ERROR "HELENGINE_CORE_CPP_ROOT does not contain runtime/runtime_scene_catalog_manifest.cpp. Regenerate helengine.core C++ output first.")
endif()

set(HELENGINE_PSP_SOURCES
    src/main.cpp
    src/generated-compat/ftruncate_shim.cpp
    src/platform/psp/PspBootHost.cpp
    src/platform/psp/PspAppRootPathResolver.cpp
    src/platform/psp/PspBootTrace.cpp
    src/platform/psp/PspInputBackend.cpp
    src/platform/psp/PspPackagedAssetLoader.cpp
    src/platform/psp/PspRuntimeSceneCatalogFactory.cpp
    src/platform/psp/rendering/PspRenderManager2D.cpp
    src/platform/psp/rendering/PspRuntimeMaterial.cpp
    src/platform/psp/rendering/PspRuntimeModel.cpp
    src/platform/psp/rendering/PspRenderManager3D.cpp
    "${HELENGINE_CORE_CPP_ROOT}/helengine_core_amalgamated.cpp"
    "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_startup_manifest.cpp"
    "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_scene_catalog_manifest.cpp"
)

if(EXISTS "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_code_module_manifest.cpp")
    list(APPEND HELENGINE_PSP_SOURCES "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_code_module_manifest.cpp")
endif()

add_executable(${PROJECT_NAME} ${HELENGINE_PSP_SOURCES})

if(HELENGINE_PSP_ISOLATED_BOOT)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HELENGINE_PSP_ISOLATED_BOOT=1)
endif()

if(HELENGINE_PSP_ENABLE_RUNTIME_STARTUP)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=1)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    src/generated-compat
    ${HELENGINE_CORE_CPP_ROOT}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    atomic
    pspdebug
    pspdisplay
    pspge
    pspgu
    pspgum
    pspctrl
)

create_pbp_file(
    TARGET ${PROJECT_NAME}
    ICON_PATH NULL
    BACKGROUND_PATH NULL
    PREVIEW_PATH NULL
    TITLE "Helengine PSP"
    VERSION 01.00
)
