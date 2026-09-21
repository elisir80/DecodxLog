// DecoDXLog — un pannello staccato: la stessa cosa che sta nella finestra
// principale, ma in una finestra sua, che si sposta su un altro monitor e si
// ridimensiona come si vuole. La misura e la posizione restano.
import QtQuick
import QtQuick.Controls
import QtCore
import Decodium.UI

ApplicationWindow {
    id: root

    property string panelKey: ""
    property string panelTitle: ""
    property string panelSource: ""

    // Riagganciare alla finestra principale, oppure chiudere del tutto.
    signal attachRequested()
    signal closeRequested()
    // Quello che il pannello chiede e che solo la finestra principale sa fare.
    signal openQsoRequested(var id)
    signal awardRequested(string id)
    signal clusterRequested(int tab)
    signal statsRequested()
    signal rotorRequested()

    // Una misura di partenza sensata per quel pannello: il log e le schede
    // vogliono spazio, la scheda nominativo no. Poi vale quella che si sceglie.
    width: root.panelKey === "logbook" ? 1100 : root.panelKey === "tabs" ? 1000 : 760
    height: root.panelKey === "logbook" ? 700 : 520
    minimumWidth: 320
    minimumHeight: 200
    visible: true
    color: Theme.bgDeep
    title: qsTr("DecoDXLog — %1").arg(root.panelTitle)

    // Davanti a tutte le altre. Il CW nasce cosi': mentre si manipola si guarda
    // quello che esce dal decoder e si scrive nel log, e la finestra del CW non
    // deve finire dietro a quella grande. Dalla puntina nella sua testata si
    // toglie, e resta tolta.
    property bool alwaysOnTop: false
    flags: Qt.Window | (root.alwaysOnTop ? Qt.WindowStaysOnTopHint : 0)

    OnScreen { target: root }

    Settings {
        id: panelSettings
        category: "layout/panel/" + root.panelKey
        property alias windowWidth: root.width
        property alias windowHeight: root.height
        property alias windowX: root.x
        property alias windowY: root.y
    }

    Component.onCompleted: {
        // Il .ini restituisce stringhe: "true" e true sono la stessa cosa.
        const saved = panelSettings.value("alwaysOnTop", root.panelKey === "cw")
        root.alwaysOnTop = saved === true || saved === "true"
    }
    onAlwaysOnTopChanged: panelSettings.setValue("alwaysOnTop", root.alwaysOnTop)

    QsoDetailDialog { id: qsoDialog }

    Loader {
        id: holder
        anchors.fill: parent
        anchors.margins: 8
        source: root.panelSource
        onLoaded: {
            if (item.panelKey !== undefined) {
                item.panelKey = root.panelKey
                item.detached = true
            }
            // Il logbook ha il suo pulsante "Stacca": qui non serve piu'.
            if (item.showPopButton !== undefined)
                item.showPopButton = false
        }
    }

    // I segnali dei pannelli sono tanti e diversi: si ascolta quello che c'e'.
    Connections {
        target: holder.item
        ignoreUnknownSignals: true
        function onAttachRequested() { root.attachRequested() }
        function onCloseRequested() { root.closeRequested() }
        function onOpenQso(id) { if (id > 0) qsoDialog.openFor(id) }
        function onAwardRequested(id) { root.awardRequested(id) }
        function onDetailsRequested(id) { root.awardRequested(id) }
        function onStatsRequested() { root.statsRequested() }
        function onClusterRequested(tab) { root.clusterRequested(tab) }
        function onWindowRequested() { root.rotorRequested() }
        function onPopRequested() { }
    }
}
