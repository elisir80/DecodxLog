// DecoLog — la scheda DIAGNOSTICA del posto di comando: traffico seriale,
// andamento della posizione, contatori e collegamenti di rete.
// Copia di `desktop/qml/DecoRotor/DiagnosticsPage.qml`.
import QtQuick
import QtQuick.Layouts

RowLayout {
    id: page

    RotorPalette { id: rt }

    spacing: rt.spacing

    // Il gateway manda frame e storico solo a richiesta: finche' la scheda e'
    // aperta si rinfrescano da soli.
    Timer {
        interval: 1500
        repeat: true
        running: page.visible
        triggeredOnStart: true
        onTriggered: decolog.rotor.refreshDiagnostics()
    }

    // Se il gateway arriva dopo, la prima richiesta parte appena risponde.
    Connections {
        target: decolog.rotor
        function onStateChanged() {
            if (page.visible && decolog.rotor.traffic.length === 0)
                decolog.rotor.refreshDiagnostics()
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: 620
        Layout.minimumWidth: 340
        spacing: rt.spacing

        RotorTraffic {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 200
        }

        RotorHistory {
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            Layout.minimumHeight: 110
        }
    }

    ColumnLayout {
        Layout.fillHeight: true
        Layout.preferredWidth: 380
        Layout.minimumWidth: 320
        Layout.maximumWidth: 460
        spacing: rt.spacing

        RotorStats {
            Layout.fillWidth: true
            Layout.preferredHeight: 280
        }

        RotorNetwork {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 180
        }
    }
}
