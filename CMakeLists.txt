cmake_minimum_required(VERSION 3.21)

project(DecoLog
    VERSION 0.1.0
    DESCRIPTION "Station logbook for the Decodium family"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(DECOLOG_BUILD_TESTS "Build the unit tests" ON)
option(DECOLOG_BUILD_TOOLS "Build the command-line tools (UDP simulator)" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network Sql Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

# ── decodium-ui ───────────────────────────────────────────────────────────────
# Tema e componenti della famiglia Decodium. Sta in libs/ perche' e' pensato per
# uscire da qui e diventare un modulo condiviso con Decodium e DecoRTTY (Fase 0,
# §4.2): nulla la' dentro sa di DecoLog.
add_subdirectory(libs/decodium-ui)

# ── Core ──────────────────────────────────────────────────────────────────────
# Niente GUI: ADIF, database e protocollo UDP si provano da soli e serviranno
# anche a strumenti senza finestra (import da riga di comando, sync).
add_library(decolog_core STATIC
    src/core/Adif.cpp
    src/core/Adif.h
    src/core/Bands.cpp
    src/core/Bands.h
    src/core/LogDatabase.cpp
    src/core/LogDatabase.h
    src/core/WsjtxProtocol.cpp
    src/core/WsjtxProtocol.h
    src/core/UdpReceiver.cpp
    src/core/UdpReceiver.h
)
qt_add_resources(decolog_core "schema"
    PREFIX "/decolog"
    BASE db
    FILES db/schema.sql
)
target_include_directories(decolog_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(decolog_core PUBLIC Qt6::Core Qt6::Network Qt6::Sql)
if(MSVC)
    target_compile_options(decolog_core PRIVATE /W4)
else()
    target_compile_options(decolog_core PRIVATE -Wall -Wextra)
endif()

# ── Application ───────────────────────────────────────────────────────────────
qt_add_executable(decolog
    src/main.cpp
    src/app/QsoTableModel.cpp
    src/app/QsoTableModel.h
    src/app/DecoLogController.cpp
    src/app/DecoLogController.h
)

set(DECOLOG_QML_FILES
    qml/DecoLog/Main.qml
    qml/DecoLog/TopBar.qml
    qml/DecoLog/IncomingPanel.qml
    qml/DecoLog/LogTablePanel.qml
    qml/DecoLog/CallInfoPanel.qml
    qml/DecoLog/BottomPanel.qml
    qml/DecoLog/StatusRail.qml
    qml/DecoLog/SettingsDialog.qml
)

qt_add_qml_module(decolog
    URI DecoLog
    VERSION 1.0
    QML_FILES ${DECOLOG_QML_FILES}
)

# Come in DecoRTTY: il bytecode QML precompilato perde il contesto delle
# traduzioni, e un'interfaccia mezza tradotta vale meno di qualche millisecondo
# all'avvio.
foreach(file IN LISTS DECOLOG_QML_FILES)
    get_filename_component(alias ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${alias} QT_QML_SKIP_CACHEGEN TRUE)
endforeach()

target_compile_definitions(decolog PRIVATE DECOLOG_VERSION="${PROJECT_VERSION}")
target_include_directories(decolog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(decolog PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
    decolog_core
    decodium_uiplugin
)
set_target_properties(decolog PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

# ── Tests ─────────────────────────────────────────────────────────────────────
if(DECOLOG_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    foreach(name adif protocol database)
        qt_add_executable(tst_${name} tests/tst_${name}.cpp)
        target_link_libraries(tst_${name} PRIVATE decolog_core Qt6::Test)
        set_target_properties(tst_${name} PROPERTIES WIN32_EXECUTABLE OFF)
        add_test(NAME ${name} COMMAND tst_${name})
    endforeach()
endif()

# ── Tools ─────────────────────────────────────────────────────────────────────
if(DECOLOG_BUILD_TOOLS)
    # Finge di essere Decodium: manda un QSO in LoggedADIF (e/o QSOLogged) sulla
    # porta UDP. Serve a provare la catena ricezione → SQLite → tabella senza
    # radio e senza aprire Decodium.
    qt_add_executable(decolog_udpsend tools/udpsend.cpp)
    target_link_libraries(decolog_udpsend PRIVATE decolog_core)
    set_target_properties(decolog_udpsend PROPERTIES WIN32_EXECUTABLE OFF)
endif()

install(TARGETS decolog
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin)
