// Lo specchio delle finestre di Decodium: la geometria (dove cade un clic,
// quale zona si e' scelta) e il riconoscimento del programma.
#include "app/WindowMirror.h"

#include <QTest>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace decolog::app;

class TestWindowMirror : public QObject {
    Q_OBJECT

private slots:
    void recognisesDecodium()
    {
        QVERIFY(mirror::isDecodiumImage("decodium.exe"));
        QVERIFY(mirror::isDecodiumImage("Decodium-4.0.exe"));
        QVERIFY(mirror::isDecodiumImage("DECODIUM.EXE"));
        QVERIFY(!mirror::isDecodiumImage("decodium"));
        QVERIFY(!mirror::isDecodiumImage("chrome.exe"));
        QVERIFY(!mirror::isDecodiumImage("DecoDXLog.exe"));
        QVERIFY(!mirror::isDecodiumImage(""));
    }

    void keepsTheRegionInside()
    {
        QCOMPARE(mirror::clampRegion(QRectF(-0.2, 0.9, 0.5, 0.5)), QRectF(0.0, 0.5, 0.5, 0.5));
        // Mai piu' piccola di un angolino: un clic distratto non la riduce a un punto.
        const QRectF tiny = mirror::clampRegion(QRectF(0.5, 0.5, 0.0, 0.0));
        QVERIFY(tiny.width() >= 0.02 && tiny.height() >= 0.02);
        QCOMPARE(mirror::clampRegion(QRectF(0, 0, 3, 3)), QRectF(0, 0, 1, 1));
    }

    void fitsWithoutStretching()
    {
        // Un'immagine larga in un pannello alto: piena in larghezza, centrata in altezza.
        const QRectF box = mirror::fitBox(QSizeF(400, 400), QSizeF(800, 200));
        QCOMPARE(box.width(), 400.0);
        QCOMPARE(box.height(), 100.0);
        QCOMPARE(box.top(), 150.0);
        QVERIFY(mirror::fitBox(QSizeF(0, 10), QSizeF(5, 5)).isEmpty());
    }

    void mapsClicksToTheWindow()
    {
        // Mostro la meta' destra, meta' alta di una finestra 1000x800, in un
        // pannello 250x200 senza bordi.
        const QRectF region(0.5, 0.0, 0.5, 0.5);
        const QRectF box(0, 0, 250, 200);
        const QSize source(1000, 800);
        QPointF p = mirror::toSource(QPointF(0, 0), box, region, source);
        QCOMPARE(p, QPointF(500, 0));
        p = mirror::toSource(QPointF(250, 200), box, region, source);
        QCOMPARE(p, QPointF(1000, 400));
        p = mirror::toSource(QPointF(125, 100), box, region, source);
        QCOMPARE(p, QPointF(750, 200));
        // Fuori dall'immagine (nei margini): niente.
        QVERIFY(mirror::toSource(QPointF(300, 50), box, region, source).x() < 0);
        // Con i margini: il pannello e' piu' largo dell'immagine.
        const QRectF centered(50, 0, 200, 200);
        QVERIFY(mirror::toSource(QPointF(10, 10), centered, QRectF(0, 0, 1, 1), source).x() < 0);
        QCOMPARE(mirror::toSource(QPointF(150, 100), centered, QRectF(0, 0, 1, 1), source), QPointF(500, 400));
    }

    void picksARegionWithTheMouse()
    {
        // La finestra intera in un pannello 400x200 (box 400x200): si traccia
        // il quarto in alto a sinistra.
        const QRectF box(0, 0, 400, 200);
        QRectF r = mirror::toRegion(QRectF(0, 0, 200, 100), box, QRectF(0, 0, 1, 1));
        QCOMPARE(r, QRectF(0, 0, 0.5, 0.5));
        // Al contrario, da sotto a destra: la stessa zona.
        r = mirror::toRegion(QRectF(QPointF(400, 200), QPointF(200, 100)), box, QRectF(0, 0, 1, 1));
        QCOMPARE(r, QRectF(0.5, 0.5, 0.5, 0.5));
        // Dentro una zona gia' scelta: le frazioni si compongono.
        r = mirror::toRegion(QRectF(0, 0, 200, 100), box, QRectF(0.5, 0.5, 0.5, 0.5));
        QCOMPARE(r, QRectF(0.5, 0.5, 0.25, 0.25));
        // Un rettangolo fuori dal pannello non cambia niente.
        QCOMPARE(mirror::toRegion(QRectF(500, 500, 10, 10), box, QRectF(0.1, 0.1, 0.5, 0.5)), QRectF(0.1, 0.1, 0.5, 0.5));
    }

    void startsFromTheDecodiumLayout()
    {
        WindowMirror item;
        item.applyPreset("decfull");
        QCOMPARE(item.target(), mirror::kMain);
        QVERIFY(item.region().left() < 0.05);
        QVERIFY(item.region().top() > 0.3);
        item.applyPreset("decsig");
        QVERIFY(item.region().left() > 0.5);
        item.applyPreset("window");
        QCOMPARE(item.region(), QRectF(0, 0, 1, 1));
    }

    // Una finestra vera, fuori schermo: se ne prende l'immagine. Se la sessione
    // non permette di copiare finestre (un servizio senza desktop), si salta.
    void capturesARealWindow()
    {
#ifdef Q_OS_WIN
        // Quasi trasparente e senza ricevere clic: sullo schermo, perche' una
        // finestra fuori da tutti gli schermi Windows non la disegna, ma
        // nessuno la vede.
        HWND h = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TRANSPARENT, L"STATIC",
                                 L"mirror test", WS_POPUP | WS_VISIBLE | SS_WHITERECT, 0, 0, 120, 80, nullptr,
                                 nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!h)
            QSKIP("no window could be created in this session");
        SetLayeredWindowAttributes(h, 0, 1, LWA_ALPHA);
        QTest::qWait(150);
        const QImage image = mirror::capture(static_cast<qint64>(reinterpret_cast<quintptr>(h)));
        DestroyWindow(h);
        if (image.isNull())
            QSKIP("this session cannot copy windows");
        QCOMPARE(image.size(), QSize(120, 80));
        QCOMPARE(image.pixelColor(60, 40), QColor(Qt::white));
#else
        QSKIP("Windows only");
#endif
    }
};

QTEST_MAIN(TestWindowMirror)
#include "tst_windowmirror.moc"
