// decodium-ui — etichetta sopra, controllo sotto. Il controllo si dichiara come
// figlio e va sotto l'etichetta, con Layout.fillWidth.
import QtQuick
import QtQuick.Layouts
import Decodium.UI

ColumnLayout {
    property alias label: caption.text

    spacing: 4

    FieldLabel {
        id: caption
        Layout.fillWidth: true
        // L'etichetta non deve allargare la colonna: "RST SENT" e "BAND" stanno
        // in colonne della stessa misura.
        Layout.preferredWidth: 10
    }
}
