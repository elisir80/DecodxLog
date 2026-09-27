// DecoDXLog — le icone a tratto dell'orologio mondiale: alba, tramonto,
// espandi, chiudi. Disegnate, non emoji: si vedono uguali dappertutto.
import QtQuick
import QtQuick.Shapes

Shape {
    id: root
    // "sunrise", "sunset", "expand", "close"
    property string kind: "expand"
    property color color: "white"
    property real stroke: 1.6
    width: 16
    height: 16
    preferredRendererType: Shape.CurveRenderer

    readonly property real s: Math.min(width, height)

    // L'orizzonte e mezzo sole, per alba e tramonto.
    ShapePath {
        strokeColor: root.kind === "sunrise" || root.kind === "sunset" ? root.color : "transparent"
        strokeWidth: root.stroke
        fillColor: "transparent"
        capStyle: ShapePath.RoundCap
        startX: root.s * 0.08; startY: root.s * 0.78
        PathLine { x: root.s * 0.92; y: root.s * 0.78 }
        PathMove { x: root.s * 0.24; y: root.s * 0.78 }
        PathArc { x: root.s * 0.76; y: root.s * 0.78; radiusX: root.s * 0.26; radiusY: root.s * 0.26 }
    }
    // La freccia: su per l'alba, giu' per il tramonto.
    ShapePath {
        strokeColor: root.kind === "sunrise" || root.kind === "sunset" ? root.color : "transparent"
        strokeWidth: root.stroke
        fillColor: "transparent"
        capStyle: ShapePath.RoundCap
        joinStyle: ShapePath.RoundJoin
        startX: root.s * 0.5; startY: root.kind === "sunrise" ? root.s * 0.36 : root.s * 0.06
        PathLine { x: root.s * 0.5; y: root.kind === "sunrise" ? root.s * 0.06 : root.s * 0.36 }
        PathMove { x: root.s * 0.36; y: root.kind === "sunrise" ? root.s * 0.2 : root.s * 0.22 }
        PathLine { x: root.s * 0.5; y: root.kind === "sunrise" ? root.s * 0.06 : root.s * 0.36 }
        PathLine { x: root.s * 0.64; y: root.kind === "sunrise" ? root.s * 0.2 : root.s * 0.22 }
    }
    // Espandi: due angoli opposti.
    ShapePath {
        strokeColor: root.kind === "expand" ? root.color : "transparent"
        strokeWidth: root.stroke
        fillColor: "transparent"
        capStyle: ShapePath.RoundCap
        joinStyle: ShapePath.RoundJoin
        startX: root.s * 0.1; startY: root.s * 0.4
        PathLine { x: root.s * 0.1; y: root.s * 0.1 }
        PathLine { x: root.s * 0.4; y: root.s * 0.1 }
        PathMove { x: root.s * 0.6; y: root.s * 0.9 }
        PathLine { x: root.s * 0.9; y: root.s * 0.9 }
        PathLine { x: root.s * 0.9; y: root.s * 0.6 }
    }
    // Chiudi: la croce.
    ShapePath {
        strokeColor: root.kind === "close" ? root.color : "transparent"
        strokeWidth: root.stroke
        fillColor: "transparent"
        capStyle: ShapePath.RoundCap
        startX: root.s * 0.2; startY: root.s * 0.2
        PathLine { x: root.s * 0.8; y: root.s * 0.8 }
        PathMove { x: root.s * 0.8; y: root.s * 0.2 }
        PathLine { x: root.s * 0.2; y: root.s * 0.8 }
    }
}
