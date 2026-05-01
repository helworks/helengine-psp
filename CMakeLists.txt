cmake_minimum_required(VERSION 3.11)

project(helengine_psp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/platform/psp/PspBootHost.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
)

if(DEFINED HELENGINE_CORE_CPP_ROOT AND NOT HELENGINE_CORE_CPP_ROOT STREQUAL "")
    target_compile_definitions(${PROJECT_NAME} PRIVATE HELENGINE_PSP_HAS_GENERATED_CORE=1)
    target_include_directories(${PROJECT_NAME} PRIVATE ${HELENGINE_CORE_CPP_ROOT})
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE HELENGINE_PSP_HAS_GENERATED_CORE=0)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    pspdisplay
    pspge
    pspgu
)

create_pbp_file(
    TARGET ${PROJECT_NAME}
    ICON_PATH NULL
    BACKGROUND_PATH NULL
    PREVIEW_PATH NULL
    TITLE "Helengine PSP"
    VERSION 01.00
)
