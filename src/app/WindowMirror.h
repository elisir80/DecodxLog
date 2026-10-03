// DecoDXLog — lo specchio di una finestra di Decodium dentro il log.
//
// Le liste di Decodium (Full Spectrum, Signal RX, la cascata…) sono finestre
// sue: qui se ne mostra una, viva, dentro un pannello della lavagna, con gli
// stessi colori, le stesse colonne, le stesse righe che vede Decodium. Niente
// di ricostruito: e' la sua finestra, o una zona della sua finestra principale.
//
// Come funziona, e cosa NON fa. La finestra di Decodium resta sua: non la si
// incorpora, non la si riparenta, non le si cambia lo stile. Windows ne tiene
// una copia (la stessa che usa per le anteprime della barra) e ogni mezzo
// secondo se ne prende un'immagine, che si ritaglia e si mostra. Per questo:
//   - la finestra deve esserci (non ridotta a icona), ma puo' stare coperta o su
//     un altro schermo;
//   - se Decodium si blocca, il pannello si ferma; se DecoDXLog si blocca o
//     cade, Decodium non se ne accorge;
//   - i clic arrivano a Decodium solo se si accende l'interruttore apposta.
// Solo Windows; altrove il pannello lo dice.
#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QTimer>
#include <QVariantList>

namespace decolog::app {

// Le finestre che si possono specchiare: quelle dei programmi Decodium aperti.
struct MirrorWindow {
    qint64 handle{0};
    QString title;
    QSize size;
    bool owned{false};      // una finestra staccata, non la principale
};

namespace mirror {

// Il target di default: la finestra principale di Decodium, qualunque titolo
// abbia in quel momento (il titolo dice il modo e il nominativo).
inline const QString kMain = QStringLiteral("@main");

// Il programma e' di Decodium: decodium.exe, decodium-qualcosa.exe.
bool isDecodiumImage(const QString& imageName);
// Una zona espressa in frazioni (0..1) della finestra, tenuta dentro i bordi e
// non piu' piccola di un minimo: 2% per lato.
QRectF clampRegion(const QRectF& region);
// Dove cade, in pixel dell'immagine intera, un punto del pannello: `box` e'
// il rettangolo in cui l'immagine e' disegnata, `region` la zona mostrata.
// Torna un punto fuori dal rettangolo se il punto e' fuori dall'immagine.
QPointF toSource(const QPointF& point, const QRectF& box, const QRectF& region, const QSize& sourceSize);
// L'inverso, per le zone scelte col mouse: da un rettangolo del pannello alla
// zona in frazioni.
QRectF toRegion(const QRectF& selection, const QRectF& box, const QRectF& region);
// Dove si disegna la zona dentro il pannello, a misura intera senza deformare.
QRectF fitBox(const QSizeF& area, const QSizeF& content);

// L'elenco, con la principale per prima. Vuoto fuori da Windows.
QList<MirrorWindow> windows();
// Una finestra per il target ("@main" o il titolo esatto), 0 se non c'e'.
qint64 resolve(const QString& target);
// Riporta una finestra ridotta a icona alla sua misura, in fondo a tutte le
// altre e senza passarle il fuoco: si vede (per lo specchio) ma non copre il
// lavoro. Un'operazione sola, chiesta dall'operatore.
bool restoreBehind(qint64 handle);
bool isMinimized(qint64 handle);
// L'immagine intera della finestra; vuota se non si puo' (ridotta a icona,
// chiusa, nessun Windows).
QImage capture(qint64 handle);

} // namespace mirror

// Chi sa prendere le immagini: una sola per finestra, per quanti pannelli la
// guardano, e mai sul filo dell'interfaccia (costa una settantina di
// millisecondi a immagine).
class MirrorHub : public QObject {
    Q_OBJECT

public:
    static MirrorHub* instance();

    // Un pannello comincia o smette di guardare un target.
    void watch(const QString& target);
    void unwatch(const QString& target);
    // L'ultima immagine presa e quanti anni ha.
    QImage frame(const QString& target) const;
    qint64 handleOf(const QString& target) const;
    bool minimizedFlag(const QString& target) const;
    // Subito un'immagine nuova (dopo un clic il risultato si vuole vedere).
    void refreshSoon(const QString& target);

    // Ogni quanto si guarda: le liste cambiano ogni periodo, non ogni frame.
    static constexpr int kIntervalMs = 500;

signals:
    void updated(const QString& target);

private:
    MirrorHub();
    struct State {
        int users{0};
        qint64 handle{0};
        bool minimized{false};
        bool busy{false};
        QImage image;
        qint64 at{0};
    };
    void tick();
    void capture(const QString& target);

    QHash<QString, State> m_state;
    QTimer m_timer;
};

class WindowMirror : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(QRectF region READ region WRITE setRegion NOTIFY regionChanged)
    Q_PROPERTY(QString settingsKey READ settingsKey WRITE setSettingsKey NOTIFY settingsKeyChanged)
    Q_PROPERTY(bool forwardInput READ forwardInput WRITE setForwardInput NOTIFY forwardInputChanged)
    Q_PROPERTY(bool matchSize READ matchSize WRITE setMatchSize NOTIFY matchSizeChanged)
    // 0 nessuna scelta, 1 si vede, 2 la finestra non c'e', 3 ridotta a icona, 4 non si puo' (non Windows)
    Q_PROPERTY(int status READ status NOTIFY statusChanged)
    Q_PROPERTY(QSize sourceSize READ sourceSize NOTIFY statusChanged)
    Q_PROPERTY(QRectF paintedRect READ paintedRect NOTIFY paintedRectChanged)
    Q_PROPERTY(bool supported READ supported CONSTANT)
    // Per scegliere la zona: si vede la finestra intera, senza toccare quella salvata.
    Q_PROPERTY(bool showAll READ showAll WRITE setShowAll NOTIFY showAllChanged)

public:
    enum Status { NoTarget = 0, Live = 1, NotFound = 2, Minimized = 3, Unsupported = 4 };
    Q_ENUM(Status)

    explicit WindowMirror(QQuickItem* parent = nullptr);
    ~WindowMirror() override;

    void paint(QPainter* painter) override;

    QString target() const { return m_target; }
    void setTarget(const QString& target);
    QRectF region() const { return m_region; }
    void setRegion(const QRectF& region);
    QString settingsKey() const { return m_settingsKey; }
    void setSettingsKey(const QString& key);
    bool forwardInput() const { return m_forward; }
    void setForwardInput(bool on);
    bool matchSize() const { return m_matchSize; }
    void setMatchSize(bool on);
    int status() const { return m_status; }
    QSize sourceSize() const { return m_image.size(); }
    QRectF paintedRect() const { return m_box; }
    bool supported() const;
    bool showAll() const { return m_showAll; }
    void setShowAll(bool on);
    // La zona che si vede adesso: la scelta, o la finestra intera.
    QRectF shownRegion() const { return m_showAll ? QRectF(0, 0, 1, 1) : m_region; }

    // Le finestre che si possono scegliere: [{target, label, width, height}].
    Q_INVOKABLE QVariantList windows() const;
    // Una zona scelta col mouse sul pannello (coordinate dell'item).
    Q_INVOKABLE void selectRegion(const QRectF& selection);
    // I valori di partenza di un pannello: "decfull", "decsig", altro = tutta la finestra.
    Q_INVOKABLE void applyPreset(const QString& name);
    // La finestra di Decodium ridotta a icona non si puo' copiare: la si
    // riporta alla misura normale, ma sotto le altre.
    Q_INVOKABLE bool restoreSource();
    // Clic e rotella verso la finestra di Decodium (solo con forwardInput).
    // kind: "press", "release", "double", "wheel"; button: 1 sinistro, 2 destro.
    Q_INVOKABLE bool send(const QString& kind, qreal x, qreal y, int button, int delta);

signals:
    void targetChanged();
    void regionChanged();
    void settingsKeyChanged();
    void forwardInputChanged();
    void matchSizeChanged();
    void statusChanged();
    void paintedRectChanged();
    void showAllChanged();

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;

private:
    void attach();
    void detach();
    void onUpdated(const QString& target);
    void recompute();
    void load();
    void save() const;
    void fitSoon();
    void setStatus(int status);

    QString m_target;
    QRectF m_region{0, 0, 1, 1};
    QString m_settingsKey;
    bool m_forward{false};
    bool m_matchSize{false};
    int m_status{NoTarget};
    QImage m_image;
    QRectF m_box;
    bool m_attached{false};
    bool m_loading{false};
    bool m_showAll{false};
    QTimer m_fitTimer;
};

} // namespace decolog::app
