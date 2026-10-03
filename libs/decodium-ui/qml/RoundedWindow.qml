// decodium-ui — base comune per finestre e dialoghi con angoli reali.
//
// Le finestre senza barra nativa hanno bisogno della loro superficie: su macOS
// (e con DWM su Windows) gli angoli esterni sono davvero trasparenti, non un
// rettangolo scuro con un radius disegnato sopra. Su Linux si conserva una
// superficie opaca, per non dipendere dal compositor della sessione.
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import Decodium.UI

ApplicationWindow {
    id: root

    property color surfaceColor: Theme.bgDeep
    // La finestra principale mantiene la sua decorazione nativa: macOS le
    // fornisce gia' forma e ombra corrette. Le finestre staccate, invece,
    // impostano la loro superficie trasparente per disegnare gli angoli veri.
    property bool transparentFrame: true
    readonly property bool roundedFrame: root.visibility !== Window.Maximized
                                       && root.visibility !== Window.FullScreen
    readonly property bool transparentCorners: root.transparentFrame && root.roundedFrame
                                               && Qt.platform.os !== "linux"
    // La maschera e' necessaria soltanto dove la finestra ha angoli realmente
    // trasparenti. Su Linux la superficie resta opaca, senza chiedere nulla al
    // compositor della sessione (importante per KDE e driver meno recenti).
    // Il layer rimane invece attivo anche durante il cambio massimizzata ↔
    // normale: ricrearlo mentre la sorgente della maschera era invisibile
    // lasciava talvolta una texture nera su Metal/D3D/OpenGL.
    readonly property bool useWindowLayer: root.transparentFrame && Qt.platform.os !== "linux"
    readonly property bool useRoundedMask: root.transparentCorners
    readonly property real frameRadius: root.roundedFrame
                                       ? (Qt.platform.os === "osx" ? 12 : 8) : 0

    // ApplicationWindow.contentItem is read-only in Qt 6. Redirect regular
    // child content into this clipped surface instead, retaining the natural
    // RoundedWindow { Item { ... } } syntax for every existing window.
    default property alias windowContent: contentScene.data

    // `color` e' il colore della QQuickWindow, non solo uno sfondo QML: deve
    // essere trasparente perche' macOS possa mostrare gli angoli veri.
    color: root.transparentCorners ? "transparent" : root.surfaceColor

    background: Rectangle {
        color: root.surfaceColor
        radius: root.frameRadius
        antialiasing: root.frameRadius > 0
    }

    // Questa e' sempre la scena reale e visibile della finestra. La maschera
    // viene applicata al suo layer, non da un secondo Item che la cattura come
    // texture. Cosi' un resize o il ripristino dalla massimizzazione non puo'
    // perdere il contenuto del pannello.
    Item {
        id: contentScene
        anchors.fill: parent
        layer.enabled: root.useWindowLayer
        layer.smooth: true
        layer.effect: MultiEffect {
            maskEnabled: root.useRoundedMask
            maskSource: cornerMask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 0.02
            maskThresholdMax: 1.0
            maskSpreadAtMax: 0.0
        }
    }

    // Rectangle.clip ritaglia sempre un rettangolo. La maschera usa invece
    // l'alpha della forma arrotondata: header, footer e qualunque contenuto
    // non possono comparire negli angoli trasparenti della QQuickWindow.
    Rectangle {
        id: cornerMask
        anchors.fill: contentScene
        radius: root.frameRadius
        color: "white"
        visible: false
        layer.enabled: root.useWindowLayer
        layer.smooth: true
        antialiasing: root.frameRadius > 0
    }
}
