# DecoLog Live — il programma vero dentro il browser

Due strade portano il log online, e non sono la stessa cosa:

| | **DecoLog Cloud** (`server/`) | **DecoLog Live** (questa cartella) |
|---|---|---|
| Cos'e' | una pagina che rifa' la finestra di DecoLog | **DecoLog**, compilato e in esecuzione sul server |
| Si scrive? | no: si guarda, si scarica, si cambiano le impostazioni | **si', tutto**: e' il programma |
| Quanti insieme | tutti i radioamatori che vuoi | una scrivania per container |
| Costa | niente, e' una pagina | ~700 MB di immagine, 300-500 MB di RAM, CPU per disegnare |
| Va sul telefono | si' | si', ma si comanda un programma da scrivania col dito |

Il Cloud resta la strada normale. Live serve quando si vuole **il programma**,
non una copia: tutte le finestre, tutte le impostazioni, i diplomi con i loro
filtri, il Nuovo QSO, l'import ADIF.

## Come funziona

Xvfb fa uno schermo finto, DecoLog ci disegna sopra (OpenGL via software,
llvmpipe), x11vnc lo pubblica e noVNC lo porta nel browser. Sullo schermo c'e'
DecoLog perche' *e'* DecoLog: lo stesso sorgente che sta su GitHub, compilato
dentro l'immagine.

## Quello che non c'e', e non e' un difetto

* **I QSO da Decodium** arrivano in UDP dal computer di casa: il server non li
  sente. Il log si riempie dal **sync**, che e' il suo mestiere.
* **Seriale, rotore, audio**: sono fili, e i fili stanno in stazione.
* **Una scrivania per volta.** Due persone collegate allo stesso container si
  vedono i movimenti a vicenda: per due radioamatori, due container.

## Accenderlo

```bash
docker build -t decolog-live -f server/live/Dockerfile .

docker run -d --name decolog-live \
  -p 127.0.0.1:6080:6080 \
  -e DECOLOG_VNC_PASSWORD='una password lunga' \
  -v decolog-live-iu8lmc:/home/decolog \
  --restart unless-stopped \
  decolog-live
```

Senza `DECOLOG_VNC_PASSWORD` il container si ferma da solo: una scrivania aperta
non si lascia in giro.

Il volume tiene il log (`/home/decolog/decolog.sqlite`) e le impostazioni: si
spegne e si riaccende senza perdere niente.

## Davanti ci va nginx

noVNC ascolta solo su 127.0.0.1: da fuori ci si arriva via HTTPS, e **con una
seconda password**, perche' una scrivania e' piu' di un log in sola lettura.

```nginx
location /live/ {
    auth_basic           "DecoLog";
    auth_basic_user_file /etc/nginx/decolog-live.htpasswd;

    proxy_pass http://127.0.0.1:6080/;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;     # il VNC viaggia su WebSocket
    proxy_set_header Connection "upgrade";
    proxy_read_timeout 12h;                     # una sessione dura quanto serve
}
```

Poi si apre `https://cloud.ft2.it/live/vnc.html?path=live/websockify&autoconnect=1`.

## Misure

* `DECOLOG_SCREEN` — quanto e' grande lo schermo finto (`1600x900x24`).
* `DECOLOG_DB` — dove sta il log dentro il container.
* Tutto quello che DecoLog accetta da riga di comando si passa in coda al
  comando: `docker run ... decolog-live --theme Darkcodium`.

## Il primo giro

1. Si apre `/live/`, e c'e' DecoLog.
2. Impostazioni → Sync e Cloud: si mette `https://cloud.ft2.it`, nominativo e
   password, e si entra.
3. Il log arriva dal Cloud — QSO, profili, impostazioni — e da quel momento
   questa scrivania e' un dispositivo come gli altri: quello che si scrive qui
   torna a casa al giro dopo.
