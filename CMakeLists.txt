cmake_minimum_required(VERSION 3.20)

project(session-dualsense-mod LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_VIGEM "Enable ViGEm support" ON)

if(WIN32)
    add_compile_definitions(
        _WIN32
        UNICODE
        _UNICODE
        NOMINMAX
    )

    if(USE_VIGEM)
        add_compile_definitions(USE_VIGEM BUILD_GAMEPAD_CORE_TESTS)
    endif()

    set(SOURCES
        src/session-dualsense-mod.cpp
        src/Platform_Windows/test_windows_device_info.cpp
        src/Platform_Windows/ViGEmAdapter/ViGEmAdapter.cpp
    )

    set(GAMEPAD_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/Gamepad-Core")
    set(BUILD_TESTS OFF CACHE BOOL "Build integration tests" FORCE)
    add_subdirectory(${GAMEPAD_CORE_DIR})

    add_library(session-dualsense-mod SHARED ${SOURCES})

    add_executable(test-device-initialization 
        src/test-device-initialization.cpp
        src/Platform_Windows/test_windows_device_info.cpp
    )

    target_include_directories(session-dualsense-mod PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Platform_Windows
        ${GAMEPAD_CORE_DIR}/Source/Public
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/ViGEmClient/include
        ${GAMEPAD_CORE_DIR}/Examples
    )

    # Hack to support relative include in session-dualsense-mod.cpp:81 and 88
    target_include_directories(session-dualsense-mod PRIVATE
        ${GAMEPAD_CORE_DIR}/Source/Public/GImplementations/Utils
        ${GAMEPAD_CORE_DIR}/Examples/Platform_Windows
    )

    # Configuração de Bibliotecas (Lib)
    target_link_directories(session-dualsense-mod PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib)
    target_link_directories(test-device-initialization PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib)

    # Linkagem de dependências
    target_link_libraries(session-dualsense-mod PRIVATE
        GamepadCore
        ViGEmClient
        Setupapi
        Hid
        dxgi
        ole32
        oleaut32
        uuid
        winmm
        shlwapi
    )

    target_include_directories(test-device-initialization PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Platform_Windows
        ${GAMEPAD_CORE_DIR}/Source/Public
        ${GAMEPAD_CORE_DIR}/Examples
    )

    target_link_libraries(test-device-initialization PRIVATE
        GamepadCore
        Setupapi
        Hid
        winmm
        shlwapi
    )

    set_target_properties(session-dualsense-mod PROPERTIES
        OUTPUT_NAME "session-dualsense-mod"
        PREFIX ""
    )

#    add_custom_command(TARGET session-dualsense-mod POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_if_different
#        "${CMAKE_CURRENT_SOURCE_DIR}/src/config.ini"
#        $<TARGET_FILE_DIR:session-dualsense-mod>/config.ini
#    )

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(ZIP_NAME "session-dualsense-mod.zip")
        set(BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin")

        add_custom_command(TARGET session-dualsense-mod POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Create package zip from Release..."
            COMMAND ${CMAKE_COMMAND} -E make_directory "${BIN_DIR}"
            COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:session-dualsense-mod>/zip_staging"
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:session-dualsense-mod>/zip_staging/UnrealModPlugins"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:session-dualsense-mod>" "$<TARGET_FILE_DIR:session-dualsense-mod>/zip_staging/UnrealModPlugins/session-dualsense-mod.dll"
#            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/config.ini" "$<TARGET_FILE_DIR:session-dualsense-mod>/zip_staging/UnrealModPlugins/config.ini"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/bin/template/dxgi.dll" "$<TARGET_FILE_DIR:session-dualsense-mod>/zip_staging/dxgi.dll"
            COMMAND powershell -Command "Compress-Archive -Path '$<TARGET_FILE_DIR:session-dualsense-mod>/zip_staging/*' -DestinationPath '${BIN_DIR}/${ZIP_NAME}' -Force"
            COMMENT "Zipping Release build to bin directory"
            VERBATIM
        )
    endif()
endif()
