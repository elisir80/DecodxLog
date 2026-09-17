// decodium-ui — l'etichetta sopra un campo: maiuscolo, piccola, spaziata.
import QtQuick
import Decodium.UI

Text {
    color: Theme.textSecondary
    font.pixelSize: 11
    font.capitalization: Font.AllUppercase
    font.letterSpacing: 0.7
    elide: Text.ElideRight
}
