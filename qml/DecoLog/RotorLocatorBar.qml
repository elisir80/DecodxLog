// DecoLog — puntamento per locatore in una riga sola, in fondo alla mappa: si
// scrive il riquadro, la meta compare sulla mappa e sul quadrante, e i due tasti
// mandano l'antenna per rotta breve o per rotta lunga.
// Copia di `desktop/qml/DecoRotor/LocatorBar.qml`.
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: bar

    property bool nightMode: true

    RotorPalette { id: rt; dark: bar.nightMode }

    readonly property var rotor: decolog.rotor
    readonly property var bearing: rotor.bearing
    readonly property bool bearingValid: bearing.short_path !== undefined

    implicitHeight: 44
    color: rt.dark ? Qt.rgba(0.05, 0.08, 0.13, 0.72) : Qt.rgba(1.0, 1.0, 1.0, 0.78)

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 8

        RotorField {
            id: locatorField

            Layout.preferredWidth: 176
            implicitHeight: 32
            placeholderText: qsTr("Locatore, es. FN31pr")
            font.capitalization: Font.AllUppercase
            onTextChanged: bar.rotor.askBearing(text)
            onAccepted: bar.rotor.pointLocator(text, false)
        }

        RotorButton {
            Layout.preferredWidth: 76
            Layout.preferredHeight: 32
            text: qsTr("BREVE")
            kind: 1
            enabled: bar.bearingValid
            onClicked: bar.rotor.pointLocator(locatorField.text, false)
        }

        RotorButton {
            Layout.preferredWidth: 76
            Layout.preferredHeight: 32
            text: qsTr("LUNGA")
            enabled: bar.bearingValid
            onClicked: bar.rotor.pointLocator(locatorField.text, true)
        }

        Text {
            Layout.fillWidth: true
            text: bar.bearingValid
                  ? qsTr("breve %1° · lunga %2° · %3 km")
                        .arg(bar.bearing.short_path.toFixed(1))
                        .arg(bar.bearing.long_path.toFixed(1))
                        .arg(bar.bearing.distance_km.toFixed(0))
                  : qsTr("QTH di riferimento: %1").arg(bar.rotor.state.locator || "")
            color: bar.bearingValid ? rt.accent : rt.textDim
            font.pixelSize: rt.fontSmall
            font.family: rt.monoFamily
            elide: Text.ElideRight
        }
    }
}
