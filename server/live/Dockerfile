# DecoLog Live — il programma vero, non una pagina che gli somiglia.
#
# Compila DecoLog per Linux e lo fa girare su uno schermo finto (Xvfb) con
# OpenGL via software; quello che disegna esce da un server VNC e arriva al
# browser attraverso noVNC. Sullo schermo c'e' DecoLog: gli stessi menu, le
# stesse finestre, le stesse impostazioni, perche' *e'* DecoLog.
#
# Cosa resta fuori, ed e' bene saperlo prima di accenderlo:
#
#  * i QSO da Decodium arrivano in UDP dal computer di casa: il server non li
#    sente. Il log si riempie con il sync, non con la radio.
#  * seriale, rotore e audio non ci sono: sono fili, e i fili stanno in stazione.
#  * e' una scrivania per volta. Due persone sullo stesso schermo si vedono i
#    movimenti a vicenda: per due radioamatori servono due container.
#
# Costa: circa 700 MB di immagine e 300-500 MB di RAM per istanza, piu' la CPU
# che serve a disegnare (llvmpipe). Su un VPS piccolo, una o due istanze.
#
#   docker build -t decolog-live -f server/live/Dockerfile .
#   docker run -d --name decolog-live -p 127.0.0.1:6080:6080 \
#     -e DECOLOG_VNC_PASSWORD=... -v decolog-live-home:/home/decolog decolog-live

# ── Compilazione ──────────────────────────────────────────────────────────────
FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git ca-certificates pkg-config \
        qt6-base-dev qt6-declarative-dev qt6-tools-dev qt6-tools-dev-tools \
        qt6-l10n-tools qt6-websockets-dev qt6-positioning-dev qt6-location-dev \
        libqt6svg6-dev libgl1-mesa-dev libxkbcommon-dev libsecret-1-dev \
        libssl-dev qtkeychain-qt6-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DDECOLOG_BUILD_TESTS=OFF \
    && cmake --build build \
    && cmake --install build --prefix /opt/decolog 2>/dev/null || \
       (mkdir -p /opt/decolog/bin && cp build/decolog /opt/decolog/bin/)

# ── Esecuzione ────────────────────────────────────────────────────────────────
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        xvfb x11vnc novnc websockify openbox \
        libqt6gui6 libqt6quick6 libqt6qml6 libqt6sql6 libqt6sql6-sqlite \
        libqt6network6 libqt6websockets6 libqt6positioning6 libqt6location6 \
        qml6-module-qtquick qml6-module-qtquick-controls \
        qml6-module-qtquick-layouts qml6-module-qtquick-dialogs \
        qml6-module-qtquick-templates qml6-module-qtquick-window \
        qml6-module-qtqml-workerscript qml6-module-qtcore \
        qml6-module-qtpositioning qml6-module-qtlocation \
        qt6-qpa-plugins libqt6keychain1 \
        fonts-dejavu-core ca-certificates tini \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /opt/decolog/bin/decolog /usr/local/bin/decolog
COPY server/live/start.sh /usr/local/bin/decolog-live
RUN chmod +x /usr/local/bin/decolog-live \
    && useradd -m -u 10001 decolog

# Lo schermo finto, la sua misura, e dove sta il log.
ENV DISPLAY=:99 \
    DECOLOG_SCREEN=1600x900x24 \
    DECOLOG_HOME=/home/decolog \
    QT_QPA_PLATFORM=xcb \
    LIBGL_ALWAYS_SOFTWARE=1 \
    QSG_RHI_BACKEND=software

USER decolog
WORKDIR /home/decolog
EXPOSE 6080

ENTRYPOINT ["/usr/bin/tini", "--"]
CMD ["/usr/local/bin/decolog-live"]
