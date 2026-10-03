#include "app/WindowMirror.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QPainter>
#include <QPointer>
#include <QQuickWindow>
#include <QSettings>
#include <QThreadPool>

#include <algorithm>
#include <cmath>
#include <iterator>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace decolog::app {

namespace mirror {

bool isDecodiumImage(const QString& imageName)
{
    const QString n = imageName.trimmed().toLower();
    if (!n.endsWith(QLatin1String(".exe")))
        return false;
    // Per le prove: un altro programma al posto di Decodium (le schermate si
    // fanno anche quando Decodium e' chiuso o ridotto a icona).
    static const QString other = qEnvironmentVariable("DECODXLOG_MIRROR_APP").trimmed().toLower();
    return n.startsWith(QLatin1String("decodium")) || (!other.isEmpty() && n.startsWith(other));
}

QRectF clampRegion(const QRectF& region)
{
    constexpr qreal kMin = 0.02;
    qreal w = std::clamp(region.width(), kMin, 1.0);
    qreal h = std::clamp(region.height(), kMin, 1.0);
    const qreal x = std::clamp(region.x(), 0.0, 1.0 - w);
    const qreal y = std::clamp(region.y(), 0.0, 1.0 - h);
    return QRectF(x, y, w, h);
}

QRectF fitBox(const QSizeF& area, const QSizeF& content)
{
    if (area.width() <= 0 || area.height() <= 0 || content.width() <= 0 || content.height() <= 0)
        return {};
    const qreal scale = std::min(area.width() / content.width(), area.height() / content.height());
    const qreal w = content.width() * scale;
    const qreal h = content.height() * scale;
    return QRectF((area.width() - w) / 2, (area.height() - h) / 2, w, h);
}

QPointF toSource(const QPointF& point, const QRectF& box, const QRectF& region, const QSize& sourceSize)
{
    if (box.width() <= 0 || box.height() <= 0 || sourceSize.isEmpty())
        return QPointF(-1, -1);
    const qreal nx = (point.x() - box.left()) / box.width();
    const qreal ny = (point.y() - box.top()) / box.height();
    if (nx < 0 || nx > 1 || ny < 0 || ny > 1)
        return QPointF(-1, -1);
    return QPointF((region.x() + nx * region.width()) * sourceSize.width(),
                   (region.y() + ny * region.height()) * sourceSize.height());
}

QRectF toRegion(const QRectF& selection, const QRectF& box, const QRectF& region)
{
    if (box.width() <= 0 || box.height() <= 0)
        return region;
    const QRectF s = selection.normalized().intersected(box);
    if (s.width() <= 0 || s.height() <= 0)
        return region;
    const qreal x0 = (s.left() - box.left()) / box.width();
    const qreal y0 = (s.top() - box.top()) / box.height();
    const qreal x1 = (s.right() - box.left()) / box.width();
    const qreal y1 = (s.bottom() - box.top()) / box.height();
    return clampRegion(QRectF(region.x() + x0 * region.width(), region.y() + y0 * region.height(),
                              (x1 - x0) * region.width(), (y1 - y0) * region.height()));
}

#ifdef Q_OS_WIN

namespace {

QString imageNameOf(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return {};
    wchar_t buffer[MAX_PATH * 2];
    DWORD length = static_cast<DWORD>(std::size(buffer));
    QString name;
    if (QueryFullProcessImageNameW(process, 0, buffer, &length))
        name = QFileInfo(QString::fromWCharArray(buffer, static_cast<int>(length))).fileName();
    CloseHandle(process);
    return name;
}

QString titleOf(HWND h)
{
    wchar_t buffer[512];
    const int n = GetWindowTextW(h, buffer, static_cast<int>(std::size(buffer)));
    return QString::fromWCharArray(buffer, n);
}

QString classOf(HWND h)
{
    wchar_t buffer[256];
    const int n = GetClassNameW(h, buffer, static_cast<int>(std::size(buffer)));
    return QString::fromWCharArray(buffer, n);
}

struct Collect {
    QList<MirrorWindow>* out;
    DWORD self;
    QHash<DWORD, bool> decodium;
};

BOOL CALLBACK collectProc(HWND h, LPARAM param)
{
    auto* c = reinterpret_cast<Collect*>(param);
    if (!IsWindowVisible(h))
        return TRUE;
    const QString title = titleOf(h);
    if (title.isEmpty())
        return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid == c->self)
        return TRUE;
    // Le finestre vere di Decodium sono finestre Qt; le barre del titolo e gli
    // aiuti (_q_titlebar, IME) no.
    if (!classOf(h).startsWith(QLatin1String("Qt")))
        return TRUE;
    auto it = c->decodium.find(pid);
    if (it == c->decodium.end())
        it = c->decodium.insert(pid, isDecodiumImage(imageNameOf(pid)));
    if (!it.value())
        return TRUE;
    RECT r;
    if (!GetWindowRect(h, &r))
        return TRUE;
    MirrorWindow w;
    w.handle = static_cast<qint64>(reinterpret_cast<quintptr>(h));
    w.title = title;
    w.size = QSize(r.right - r.left, r.bottom - r.top);
    w.owned = GetWindow(h, GW_OWNER) != nullptr;
    c->out->append(w);
    return TRUE;
}

HWND toHwnd(qint64 handle)
{
    return reinterpret_cast<HWND>(static_cast<quintptr>(handle));
}

} // namespace

QList<MirrorWindow> windows()
{
    QList<MirrorWindow> out;
    Collect c{&out, GetCurrentProcessId(), {}};
    EnumWindows(collectProc, reinterpret_cast<LPARAM>(&c));
    // La principale prima (la piu' grande fra quelle senza proprietario), poi
    // le altre per titolo.
    std::stable_sort(out.begin(), out.end(), [](const MirrorWindow& a, const MirrorWindow& b) {
        if (a.owned != b.owned)
            return !a.owned;
        if (!a.owned)
            return static_cast<qint64>(a.size.width()) * a.size.height() > static_cast<qint64>(b.size.width()) * b.size.height();
        return a.title.localeAwareCompare(b.title) < 0;
    });
    return out;
}

qint64 resolve(const QString& target)
{
    if (target.isEmpty())
        return 0;
    const QList<MirrorWindow> all = windows();
    for (const MirrorWindow& w : all) {
        if (target == kMain ? !w.owned : w.title == target)
            return w.handle;
    }
    return 0;
}

bool isMinimized(qint64 handle)
{
    return handle != 0 && IsIconic(toHwnd(handle));
}

bool restoreBehind(qint64 handle)
{
    HWND h = toHwnd(handle);
    if (!h || !IsWindow(h))
        return false;
    HWND front = GetForegroundWindow();
    ShowWindow(h, SW_RESTORE);
    // In fondo, senza attivarla: l'operatore resta dove stava.
    SetWindowPos(h, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (front && front != h)
        SetForegroundWindow(front);
    return true;
}

QImage capture(qint64 handle)
{
    HWND h = toHwnd(handle);
    RECT r;
    if (!h || IsIconic(h) || !GetWindowRect(h, &r))
        return {};
    const int w = r.right - r.left;
    const int hh = r.bottom - r.top;
    if (w <= 0 || hh <= 0 || w > 16384 || hh > 16384)
        return {};
    HDC screen = GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w;
    info.bmiHeader.biHeight = -hh;      // dall'alto
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    QImage image;
    if (bitmap && bits) {
        HGDIOBJ old = SelectObject(memory, bitmap);
        // PW_RENDERFULLCONTENT (2): la copia che Windows tiene della finestra,
        // anche se e' coperta o fuori schermo.
        if (PrintWindow(h, memory, 2))
            image = QImage(static_cast<const uchar*>(bits), w, hh, w * 4, QImage::Format_RGB32).copy();
        SelectObject(memory, old);
    }
    if (bitmap)
        DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    return image;
}

#else

QList<MirrorWindow> windows() { return {}; }
qint64 resolve(const QString&) { return 0; }
bool isMinimized(qint64) { return false; }
QImage capture(qint64) { return {}; }
bool restoreBehind(qint64) { return false; }

#endif

} // namespace mirror

// ── Chi prende le immagini ───────────────────────────────────────────────────

MirrorHub* MirrorHub::instance()
{
    static MirrorHub* hub = new MirrorHub();
    return hub;
}

MirrorHub::MirrorHub()
    : QObject(QCoreApplication::instance())
{
    m_timer.setInterval(kIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &MirrorHub::tick);
}

void MirrorHub::watch(const QString& target)
{
    if (target.isEmpty())
        return;
    ++m_state[target].users;
    if (!m_timer.isActive())
        m_timer.start();
    QTimer::singleShot(0, this, [this, target] { capture(target); });
}

void MirrorHub::unwatch(const QString& target)
{
    auto it = m_state.find(target);
    if (it == m_state.end())
        return;
    if (--it->users <= 0)
        m_state.erase(it);
    if (m_state.isEmpty())
        m_timer.stop();
}

QImage MirrorHub::frame(const QString& target) const
{
    return m_state.value(target).image;
}

qint64 MirrorHub::handleOf(const QString& target) const
{
    return m_state.value(target).handle;
}

bool MirrorHub::minimizedFlag(const QString& target) const
{
    return m_state.value(target).minimized;
}

void MirrorHub::refreshSoon(const QString& target)
{
    QTimer::singleShot(120, this, [this, target] { capture(target); });
}

void MirrorHub::tick()
{
    const QStringList targets = m_state.keys();
    for (const QString& target : targets)
        capture(target);
}

void MirrorHub::capture(const QString& target)
{
    auto it = m_state.find(target);
    if (it == m_state.end() || it->busy)
        return;
    const qint64 handle = mirror::resolve(target);
    it->handle = handle;
    it->minimized = handle != 0 && mirror::isMinimized(handle);
    if (handle == 0 || it->minimized) {
        // Niente finestra, o ridotta a icona: l'ultima immagine non e' piu'
        // vera. Si tiene solo se e' ridotta a icona (torna com'era).
        if (handle == 0)
            it->image = QImage();
        emit updated(target);
        return;
    }
    it->busy = true;
    QPointer<MirrorHub> self(this);
    QThreadPool::globalInstance()->start([self, target, handle] {
        const QImage image = mirror::capture(handle);
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, target, image] {
                if (!self)
                    return;
                auto at = self->m_state.find(target);
                if (at == self->m_state.end())
                    return;
                at->busy = false;
                if (!image.isNull()) {
                    at->image = image;
                    at->at = QDateTime::currentMSecsSinceEpoch();
                }
                emit self->updated(target);
            },
            Qt::QueuedConnection);
    });
}

// ── L'elemento ───────────────────────────────────────────────────────────────

WindowMirror::WindowMirror(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(false);
    m_fitTimer.setSingleShot(true);
    m_fitTimer.setInterval(400);
    connect(&m_fitTimer, &QTimer::timeout, this, [this] {
#ifdef Q_OS_WIN
        // La finestra staccata di Decodium si porta alla misura che serve a
        // mostrarla 1:1: il testo resta leggibile, e le colonne si rimettono a
        // posto da sole. Mai la principale.
        if (!m_matchSize || m_target == mirror::kMain || m_target.isEmpty() || !window())
            return;
        const qint64 handle = MirrorHub::instance()->handleOf(m_target);
        if (handle == 0 || width() < 40 || height() < 40)
            return;
        const qreal dpr = window()->devicePixelRatio();
        const QRectF shown = shownRegion();
        const int w = static_cast<int>(std::lround(width() * dpr / shown.width()));
        const int h = static_cast<int>(std::lround(height() * dpr / shown.height()));
        SetWindowPos(reinterpret_cast<HWND>(static_cast<quintptr>(handle)), nullptr, 0, 0, w, h,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
        MirrorHub::instance()->refreshSoon(m_target);
#endif
    });
}

WindowMirror::~WindowMirror()
{
    detach();
}

bool WindowMirror::supported() const
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

void WindowMirror::setStatus(int status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void WindowMirror::attach()
{
    if (m_attached || m_target.isEmpty() || !isVisible())
        return;
    auto* hub = MirrorHub::instance();
    hub->watch(m_target);
    connect(hub, &MirrorHub::updated, this, &WindowMirror::onUpdated);
    m_attached = true;
}

void WindowMirror::detach()
{
    if (!m_attached)
        return;
    auto* hub = MirrorHub::instance();
    disconnect(hub, &MirrorHub::updated, this, &WindowMirror::onUpdated);
    hub->unwatch(m_target);
    m_attached = false;
}

void WindowMirror::itemChange(ItemChange change, const ItemChangeData& value)
{
    QQuickPaintedItem::itemChange(change, value);
    // Si guarda la finestra solo mentre il pannello si vede: chiuso, nascosto
    // o staccato altrove, non si prende nessuna immagine.
    if (change == ItemVisibleHasChanged) {
        if (value.boolValue)
            attach();
        else
            detach();
    } else if (change == ItemSceneChange) {
        if (value.window)
            attach();
        else
            detach();
    }
}

void WindowMirror::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    recompute();
    if (m_matchSize)
        m_fitTimer.start();
}

void WindowMirror::onUpdated(const QString& target)
{
    if (target != m_target)
        return;
    auto* hub = MirrorHub::instance();
    m_image = hub->frame(target);
    if (!supported())
        setStatus(Unsupported);
    else if (m_target.isEmpty())
        setStatus(NoTarget);
    else if (hub->handleOf(target) == 0)
        setStatus(NotFound);
    else if (hub->minimizedFlag(target))
        setStatus(Minimized);
    else
        setStatus(m_image.isNull() ? NotFound : Live);
    recompute();
    update();
}

void WindowMirror::recompute()
{
    QRectF box;
    if (!m_image.isNull()) {
        const QRectF shown = shownRegion();
        const QSizeF content(m_image.width() * shown.width(), m_image.height() * shown.height());
        box = mirror::fitBox(size(), content);
    }
    if (box != m_box) {
        m_box = box;
        emit paintedRectChanged();
    }
}

void WindowMirror::paint(QPainter* painter)
{
    if (m_image.isNull() || m_box.isEmpty())
        return;
    const QRectF shown = shownRegion();
    const QRect source(static_cast<int>(std::lround(shown.x() * m_image.width())),
                       static_cast<int>(std::lround(shown.y() * m_image.height())),
                       std::max(1, static_cast<int>(std::lround(shown.width() * m_image.width()))),
                       std::max(1, static_cast<int>(std::lround(shown.height() * m_image.height()))));
    const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
    const QSize target(std::max(1, static_cast<int>(std::lround(m_box.width() * dpr))),
                       std::max(1, static_cast<int>(std::lround(m_box.height() * dpr))));
    QImage part = m_image.copy(source);
    // Ridurre di molto con un filtro semplice sbriciola il testo: il filtro
    // morbido di Qt fa la media dei pixel.
    if (part.size() != target)
        part = part.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    part.setDevicePixelRatio(dpr);
    painter->drawImage(m_box.topLeft(), part);
}

void WindowMirror::setTarget(const QString& target)
{
    if (m_target == target)
        return;
    const bool wasAttached = m_attached;
    detach();
    m_target = target;
    m_image = QImage();
    setStatus(target.isEmpty() ? NoTarget : (supported() ? NotFound : Unsupported));
    recompute();
    update();
    emit targetChanged();
    if (!m_loading)
        save();
    if (wasAttached || window())
        attach();
}

void WindowMirror::setRegion(const QRectF& region)
{
    const QRectF clamped = mirror::clampRegion(region);
    if (m_region == clamped)
        return;
    m_region = clamped;
    recompute();
    update();
    emit regionChanged();
    if (!m_loading)
        save();
}

void WindowMirror::setShowAll(bool on)
{
    if (m_showAll == on)
        return;
    m_showAll = on;
    recompute();
    update();
    emit showAllChanged();
}

void WindowMirror::setForwardInput(bool on)
{
    if (m_forward == on)
        return;
    m_forward = on;
    emit forwardInputChanged();
    if (!m_loading)
        save();
}

void WindowMirror::setMatchSize(bool on)
{
    if (m_matchSize == on)
        return;
    m_matchSize = on;
    emit matchSizeChanged();
    if (on)
        m_fitTimer.start();
    if (!m_loading)
        save();
}

void WindowMirror::setSettingsKey(const QString& key)
{
    if (m_settingsKey == key)
        return;
    m_settingsKey = key;
    emit settingsKeyChanged();
    load();
}

void WindowMirror::load()
{
    if (m_settingsKey.isEmpty())
        return;
    m_loading = true;
    QSettings s;
    s.beginGroup(QStringLiteral("mirror/") + m_settingsKey);
    if (s.contains(QStringLiteral("target"))) {
        setTarget(s.value(QStringLiteral("target")).toString());
        setRegion(QRectF(s.value(QStringLiteral("x"), 0.0).toDouble(), s.value(QStringLiteral("y"), 0.0).toDouble(),
                         s.value(QStringLiteral("w"), 1.0).toDouble(), s.value(QStringLiteral("h"), 1.0).toDouble()));
        setForwardInput(s.value(QStringLiteral("forward"), false).toBool());
        setMatchSize(s.value(QStringLiteral("match"), false).toBool());
    } else {
        // La prima volta: la zona di Decodium che questo pannello e' fatto per mostrare.
        applyPreset(m_settingsKey);
    }
    m_loading = false;
}

void WindowMirror::save() const
{
    if (m_settingsKey.isEmpty())
        return;
    QSettings s;
    s.beginGroup(QStringLiteral("mirror/") + m_settingsKey);
    s.setValue(QStringLiteral("target"), m_target);
    s.setValue(QStringLiteral("x"), m_region.x());
    s.setValue(QStringLiteral("y"), m_region.y());
    s.setValue(QStringLiteral("w"), m_region.width());
    s.setValue(QStringLiteral("h"), m_region.height());
    s.setValue(QStringLiteral("forward"), m_forward);
    s.setValue(QStringLiteral("match"), m_matchSize);
}

void WindowMirror::applyPreset(const QString& name)
{
    // Le zone di partenza sono quelle della disposizione di Decodium 4 appena
    // installato: Full Spectrum in basso a sinistra, Signal RX a destra. Se
    // l'operatore ha cambiato la disposizione, le ritaglia di nuovo.
    const bool wasLoading = m_loading;
    m_loading = true;
    setTarget(mirror::kMain);
    if (name == QLatin1String("decfull"))
        setRegion(QRectF(0.017, 0.40, 0.590, 0.60));
    else if (name == QLatin1String("decsig"))
        setRegion(QRectF(0.619, 0.40, 0.381, 0.60));
    else
        setRegion(QRectF(0, 0, 1, 1));
    m_loading = wasLoading;
    if (!m_loading)
        save();
}

QVariantList WindowMirror::windows() const
{
    QVariantList out;
    for (const MirrorWindow& w : mirror::windows()) {
        out << QVariantMap{
            {QStringLiteral("target"), w.owned ? w.title : mirror::kMain},
            {QStringLiteral("label"), w.owned ? w.title : tr("Decodium main window")},
            {QStringLiteral("width"), w.size.width()},
            {QStringLiteral("height"), w.size.height()},
            {QStringLiteral("main"), !w.owned},
        };
    }
    return out;
}

void WindowMirror::selectRegion(const QRectF& selection)
{
    // La selezione e' sulla vista di adesso (la finestra intera, se si sta
    // scegliendo): ne esce una zona in frazioni della finestra.
    setRegion(mirror::toRegion(selection, m_box, shownRegion()));
}

bool WindowMirror::restoreSource()
{
    const qint64 handle = MirrorHub::instance()->handleOf(m_target);
    if (handle == 0)
        return false;
    // Su un altro filo: ShowWindow aspetta la finestra di Decodium, e se ha da
    // fare l'interfaccia del log non deve restare ferma ad aspettarla.
    QThreadPool::globalInstance()->start([handle] { mirror::restoreBehind(handle); });
    MirrorHub::instance()->refreshSoon(m_target);
    return true;
}

bool WindowMirror::send(const QString& kind, qreal x, qreal y, int button, int delta)
{
#ifdef Q_OS_WIN
    if (!m_forward || m_status != Live || m_image.isNull())
        return false;
    const qint64 handle = MirrorHub::instance()->handleOf(m_target);
    if (handle == 0)
        return false;
    const QPointF p = mirror::toSource(QPointF(x, y), m_box, shownRegion(), m_image.size());
    if (p.x() < 0)
        return false;
    HWND h = reinterpret_cast<HWND>(static_cast<quintptr>(handle));
    RECT wr;
    POINT origin{0, 0};
    if (!GetWindowRect(h, &wr) || !ClientToScreen(h, &origin))
        return false;
    // L'immagine e' della finestra intera; i messaggi vogliono coordinate del
    // suo interno (o, per la rotella, dello schermo).
    const int cx = static_cast<int>(std::lround(p.x())) - (origin.x - wr.left);
    const int cy = static_cast<int>(std::lround(p.y())) - (origin.y - wr.top);
    const LPARAM client = MAKELPARAM(static_cast<WORD>(cx), static_cast<WORD>(cy));
    const bool right = button == 2;
    if (kind == QLatin1String("wheel")) {
        const LPARAM screen = MAKELPARAM(static_cast<WORD>(wr.left + std::lround(p.x())),
                                         static_cast<WORD>(wr.top + std::lround(p.y())));
        PostMessageW(h, WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(static_cast<short>(delta))), screen);
    } else if (kind == QLatin1String("press") || kind == QLatin1String("double")) {
        // Il puntatore prima arriva, poi preme: Qt Quick sceglie la riga dal
        // punto del movimento.
        PostMessageW(h, WM_MOUSEMOVE, 0, client);
        const UINT msg = kind == QLatin1String("double") ? (right ? WM_RBUTTONDBLCLK : WM_LBUTTONDBLCLK)
                                                          : (right ? WM_RBUTTONDOWN : WM_LBUTTONDOWN);
        PostMessageW(h, msg, right ? MK_RBUTTON : MK_LBUTTON, client);
    } else if (kind == QLatin1String("release")) {
        PostMessageW(h, right ? WM_RBUTTONUP : WM_LBUTTONUP, 0, client);
    } else {
        return false;
    }
    // Dopo un clic il risultato si vuole vedere subito, non fra mezzo secondo.
    MirrorHub::instance()->refreshSoon(m_target);
    return true;
#else
    Q_UNUSED(kind) Q_UNUSED(x) Q_UNUSED(y) Q_UNUSED(button) Q_UNUSED(delta)
    return false;
#endif
}

} // namespace decolog::app
