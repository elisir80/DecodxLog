# decodium-ui — tema e componenti comuni della famiglia Decodium.
#
# Candidato a modulo condiviso (sottomodulo git o libreria CMake) fra Decodium,
# DecoRTTY e DecoLog. Per questo non dipende da nulla del programma che lo
# ospita: i valori dei temi sono quelli di DecodiumThemeManager, le impostazioni
# vanno nel QSettings dell'applicazione che lo usa.

set(DECODIUM_UI_QML_FILES
    qml/GlassPanel.qml
    qml/PanelHeader.qml
    qml/PanelControl.qml
    qml/GlassButton.qml
    qml/StyledComboBox.qml
    qml/StyledTextField.qml
    qml/StatusItem.qml
    qml/Pill.qml
    qml/FieldLabel.qml
    qml/LabeledField.qml
    qml/StatTile.qml
    qml/MeterBar.qml
    qml/ToggleSwitch.qml
    qml/RadioChoice.qml
    qml/SectionTitle.qml
    qml/TabChip.qml
    qml/DialogFrame.qml
    qml/Led.qml
    qml/StyledMenu.qml
    qml/StyledMenuItem.qml
)

qt_add_qml_module(decodium_ui
    URI Decodium.UI
    VERSION 1.0
    STATIC
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/Decodium/UI
    SOURCES
        src/ThemeManager.cpp
        src/ThemeManager.h
    QML_FILES ${DECODIUM_UI_QML_FILES}
)
# I file stanno in qml/ ma nel modulo compaiono alla radice, accanto al qmldir.
foreach(file IN LISTS DECODIUM_UI_QML_FILES)
    get_filename_component(alias ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${alias} QT_QML_SKIP_CACHEGEN TRUE)
endforeach()

target_include_directories(decodium_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(decodium_ui PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
