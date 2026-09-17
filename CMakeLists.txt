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

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network Sql Gui Qml Quick QuickControls2 LinguistTools)
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
    src/core/Awards.cpp
    src/core/Awards.h
    src/core/Bands.cpp
    src/core/Bands.h
    src/core/Callbook.cpp
    src/core/Callbook.h
    src/core/Countries.cpp
    src/core/Countries.h
    src/core/DecoLinkServer.cpp
    src/core/DecoLinkServer.h
    src/core/ClusterConnection.cpp
    src/core/ClusterConnection.h
    src/core/CredentialStore.cpp
    src/core/CredentialStore.h
    src/core/Maidenhead.cpp
    src/core/Maidenhead.h
    src/core/LogDatabase.cpp
    src/core/LogDatabase.h
    src/core/Lotw.cpp
    src/core/Lotw.h
    src/core/NetworkError.h
    src/core/Spots.cpp
    src/core/Spots.h
    src/core/WsjtxProtocol.cpp
    src/core/WsjtxProtocol.h
    src/core/UdpReceiver.cpp
    src/core/UdpReceiver.h
    src/core/VoiceAnnouncer.cpp
    src/core/VoiceAnnouncer.h
)
qt_add_resources(decolog_core "schema"
    PREFIX "/decolog"
    BASE db
    FILES db/schema.sql
)
# Elenco delle entita' DXCC di AD1C (country-files.com), con i numeri ADIF.
qt_add_resources(decolog_core "cty"
    PREFIX "/decolog"
    BASE resources/cty
    FILES resources/cty/cty.csv
)
target_include_directories(decolog_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(decolog_core PUBLIC Qt6::Core Qt6::Network Qt6::Sql)
# Annunci vocali: su Windows la sintesi del sistema (SAPI).
if(WIN32)
    target_link_libraries(decolog_core PRIVATE sapi ole32)
endif()

# Portachiavi di sistema per password e chiavi API. Senza qtkeychain DecoLog si
# compila lo stesso, ma non salva segreti: niente ripiego in chiaro.
#   MSYS2:  pacman -S mingw-w64-x86_64-qtkeychain
#   Debian: apt install qtkeychain-qt6-dev
find_package(Qt6Keychain QUIET)
if(TARGET qt6keychain)
    target_link_libraries(decolog_core PUBLIC qt6keychain)
    target_compile_definitions(decolog_core PUBLIC DECOLOG_HAVE_KEYCHAIN=1)
    message(STATUS "DecoLog: qtkeychain found — credentials in the system keystore")
else()
    message(WARNING "DecoLog: qtkeychain not found — credentials cannot be stored")
endif()
if(MSVC)
    target_compile_options(decolog_core PRIVATE /W4)
else()
    target_compile_options(decolog_core PRIVATE -Wall -Wextra)
endif()

# ── Application ───────────────────────────────────────────────────────────────
# Icona e versione dell'eseguibile su Windows (resources/make_icon.py disegna
# l'icona; il .rc prende i numeri dal progetto).
if(WIN32)
    configure_file(resources/decolog.rc.in ${CMAKE_CURRENT_BINARY_DIR}/decolog.rc @ONLY)
    set(DECOLOG_WIN_RESOURCES ${CMAKE_CURRENT_BINARY_DIR}/decolog.rc)
endif()

qt_add_executable(decolog
    ${DECOLOG_WIN_RESOURCES}
    src/main.cpp
    src/app/QsoTableModel.cpp
    src/app/QsoTableModel.h
    src/app/DecoLogController.cpp
    src/app/DecoLogController.h
    src/app/StationProfileModel.cpp
    src/app/StationProfileModel.h
    src/app/ClusterController.cpp
    src/app/ClusterController.h
    src/app/SpotModel.cpp
    src/app/SpotModel.h
)

set(DECOLOG_QML_FILES
    qml/DecoLog/Main.qml
    qml/DecoLog/TopBar.qml
    qml/DecoLog/NewQsoPanel.qml
    qml/DecoLog/LogbookPanel.qml
    qml/DecoLog/CallInfoPanel.qml
    qml/DecoLog/Ft2AwardPanel.qml
    qml/DecoLog/BottomTabs.qml
    qml/DecoLog/MapPanel.qml
    qml/DecoLog/StatusRail.qml
    qml/DecoLog/NewQsoDialog.qml
    qml/DecoLog/QsoDetailDialog.qml
    qml/DecoLog/StationProfilesDialog.qml
    qml/DecoLog/SetupDialog.qml
    qml/DecoLog/LogbookWindow.qml
    qml/DecoLog/CredentialsList.qml
    qml/DecoLog/AwardsDialog.qml
    qml/DecoLog/GridSquareMap.qml
    qml/DecoLog/ClusterPanel.qml
    qml/DecoLog/ClusterWindow.qml
    qml/DecoLog/SpotFilterEditor.qml
)

qt_add_qml_module(decolog
    URI DecoLog
    VERSION 1.0
    QML_FILES ${DECOLOG_QML_FILES}
)

# Traduzioni: lrelease le compila e le mette fra le risorse (:/i18n). lupdate si
# lancia a mano quando cambiano le stringhe:
#   lupdate -locations none -no-obsolete -recursive src qml libs/decodium-ui \
#           -ts translations/decolog_it.ts
qt_add_translations(decolog TS_FILES translations/decolog_it.ts)

# L'icona della finestra (la barra delle applicazioni su Windows usa quella del
# .rc, ma la finestra e i dialoghi prendono questa).
qt_add_resources(decolog "icon"
    PREFIX "/decolog"
    BASE resources
    FILES resources/decolog.png
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
    decodium_ui
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
    foreach(name adif protocol database countries credentials callbook awards decolink lotw spots cluster)
        qt_add_executable(tst_${name} tests/tst_${name}.cpp)
        target_link_libraries(tst_${name} PRIVATE decolog_core Qt6::Test)
        set_target_properties(tst_${name} PROPERTIES WIN32_EXECUTABLE OFF)
        add_test(NAME ${name} COMMAND tst_${name})
    endforeach()
    # La tabella del log sta nell'applicazione, ma i suoi filtri sono SQL da provare.
    qt_add_executable(tst_qsomodel tests/tst_qsomodel.cpp src/app/QsoTableModel.cpp src/app/QsoTableModel.h)
    target_include_directories(tst_qsomodel PRIVATE src)
    target_link_libraries(tst_qsomodel PRIVATE decolog_core Qt6::Test)
    set_target_properties(tst_qsomodel PROPERTIES WIN32_EXECUTABLE OFF)
    add_test(NAME qsomodel COMMAND tst_qsomodel)
    qt_add_executable(tst_clustercontroller tests/tst_clustercontroller.cpp
        src/app/ClusterController.cpp src/app/ClusterController.h src/app/SpotModel.cpp src/app/SpotModel.h)
    target_include_directories(tst_clustercontroller PRIVATE src)
    target_link_libraries(tst_clustercontroller PRIVATE decolog_core Qt6::Test)
    set_target_properties(tst_clustercontroller PROPERTIES WIN32_EXECUTABLE OFF)
    add_test(NAME clustercontroller COMMAND tst_clustercontroller)
endif()

# ── Tools ─────────────────────────────────────────────────────────────────────
if(DECOLOG_BUILD_TOOLS)
    # Finge di essere Decodium: manda un QSO in LoggedADIF (e/o QSOLogged) sulla
    # porta UDP. Serve a provare la catena ricezione → SQLite → tabella senza
    # radio e senza aprire Decodium.
    qt_add_executable(decolog_udpsend tools/udpsend.cpp)
    target_link_libraries(decolog_udpsend PRIVATE decolog_core)
    set_target_properties(decolog_udpsend PROPERTIES WIN32_EXECUTABLE OFF)
    # Si collega a un nodo, RBN o POTA e stampa gli spot: per provare una fonte.
    qt_add_executable(decolog_clusterprobe tools/clusterprobe.cpp)
    target_link_libraries(decolog_clusterprobe PRIVATE decolog_core)
    set_target_properties(decolog_clusterprobe PROPERTIES WIN32_EXECUTABLE OFF)
endif()

install(TARGETS decolog
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin)
