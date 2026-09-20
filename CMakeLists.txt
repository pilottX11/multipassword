cmake_minimum_required(VERSION 3.21)

project(multipassword
    VERSION 1.0.0
    DESCRIPTION "multipassword - an encrypted password & crypto seed manager"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(MP_BUILD_TESTS "Build unit tests" ON)

# --- Dependencies ----------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg Network)
find_package(unofficial-sodium CONFIG QUIET)
if(unofficial-sodium_FOUND)
    set(MP_SODIUM_TARGET unofficial-sodium::sodium)
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SODIUM IMPORTED_TARGET libsodium)
    endif()
    if(TARGET PkgConfig::SODIUM)
        set(MP_SODIUM_TARGET PkgConfig::SODIUM)
    else()
        find_path(SODIUM_INCLUDE_DIR sodium.h REQUIRED)
        find_library(SODIUM_LIBRARY NAMES sodium libsodium REQUIRED)
        add_library(mp_sodium INTERFACE)
        target_include_directories(mp_sodium INTERFACE ${SODIUM_INCLUDE_DIR})
        target_link_libraries(mp_sodium INTERFACE ${SODIUM_LIBRARY})
        set(MP_SODIUM_TARGET mp_sodium)
    endif()
endif()

# --- Hardening flags -------------------------------------------------------
add_library(mp_hardening INTERFACE)
if(MSVC)
    target_compile_options(mp_hardening INTERFACE
        /W4 /permissive- /utf-8 /GS /guard:cf /sdl /Zc:__cplusplus /EHsc)
    target_link_options(mp_hardening INTERFACE
        /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /guard:cf /CETCOMPAT)
    target_compile_definitions(mp_hardening INTERFACE
        _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
else()
    target_compile_options(mp_hardening INTERFACE
        -Wall -Wextra -Wpedantic -fstack-protector-strong -D_FORTIFY_SOURCE=2
        -fPIE -fno-plt)
    target_link_options(mp_hardening INTERFACE -pie -Wl,-z,relro,-z,now)
endif()

# --- Core library (no Qt GUI dependency, only QtCore) ---------------------
add_library(mpcore STATIC
    src/core/SecureMemory.cpp
    src/core/Crypto.cpp
    src/core/VaultFormat.cpp
    src/core/Item.cpp
    src/core/Vault.cpp
    src/core/PasswordGenerator.cpp
    src/core/PasswordStrength.cpp
    src/core/Bip39.cpp
    src/core/Totp.cpp
    src/core/Settings.cpp
)
target_include_directories(mpcore PUBLIC src)
target_link_libraries(mpcore PUBLIC Qt6::Core ${MP_SODIUM_TARGET} mp_hardening)

# --- Autofill library ------------------------------------------------------
add_library(mpautofill STATIC
    src/autofill/AutofillEngine.cpp
    src/autofill/AutoType.cpp
)
target_link_libraries(mpautofill PUBLIC mpcore Qt6::Gui Qt6::Widgets)
if(WIN32)
    target_link_libraries(mpautofill PUBLIC user32 ole32 oleaut32 uiautomationcore)
endif()

# --- Application -----------------------------------------------------------
set(MP_APP_SOURCES
    src/main.cpp
    src/ui/Theme.cpp
    src/ui/Icons.cpp
    src/ui/IconBadge.cpp
    src/ui/Sidebar.cpp
    src/ui/ItemListPanel.cpp
    src/ui/ItemListDelegate.cpp
    src/ui/ItemDetailPanel.cpp
    src/ui/ItemEditPanel.cpp
    src/ui/FieldRow.cpp
    src/ui/MainWindow.cpp
    src/ui/UnlockDialog.cpp
    src/ui/SignupDialog.cpp
    src/ui/PasswordGeneratorDialog.cpp
    src/ui/SettingsDialog.cpp
    src/ui/AutofillPicker.cpp
    src/ui/ClipboardManager.cpp
    resources/resources.qrc
)

if(WIN32)
    list(APPEND MP_APP_SOURCES resources/multipassword.rc resources/multipassword.manifest)
endif()

qt_add_executable(multipassword WIN32 MACOSX_BUNDLE ${MP_APP_SOURCES})
target_link_libraries(multipassword PRIVATE
    mpcore mpautofill Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Network)
if(WIN32)
    target_link_libraries(multipassword PRIVATE user32 wtsapi32)
endif()

set_target_properties(multipassword PROPERTIES
    OUTPUT_NAME "multipassword"
    WIN32_EXECUTABLE TRUE)

# --- Tests -----------------------------------------------------------------
if(MP_BUILD_TESTS)
    enable_testing()
    add_executable(mp_tests tests/test_main.cpp resources/resources.qrc)
    target_link_libraries(mp_tests PRIVATE mpcore mpautofill)
    add_test(NAME mp_tests COMMAND mp_tests)
endif()

# --- Deployment helper (copies Qt DLLs next to the exe on Windows) ---------
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET multipassword POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --no-opengl-sw
                    --no-system-d3d-compiler "$<TARGET_FILE:multipassword>"
            COMMENT "Deploying Qt runtime next to multipassword.exe")
    endif()
endif()

# --- Developer tool: seed a demo vault (not installed) ---------------------
if(MP_BUILD_TESTS)
    add_executable(mp_seed_demo tests/seed_demo_vault.cpp resources/resources.qrc)
    target_link_libraries(mp_seed_demo PRIVATE mpcore)
endif()
