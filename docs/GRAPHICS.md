# DecoDXLog — modalità di rendering

DecoDXLog usa le stesse modalità di avvio di Qt Quick già adottate da Decodium 4.
La scelta viene applicata prima della creazione di `QGuiApplication`, quindi vale
per tutte le finestre QML.

## Modalità disponibili

| Modalità | Backend | Uso |
| --- | --- | --- |
| `auto` | Metal su macOS, D3D12 su Windows, OpenGL su Linux | avvio normale e compatibile |
| `opengl` | OpenGL / RHI | fallback consigliato su Linux con Vulkan/Mesa problematico |
| `vulkan` | Vulkan / RHI | solo con driver Vulkan stabili |
| `metal` | Metal / RHI | macOS |
| `d3d12` | Direct3D 12 / RHI | Windows moderno |
| `d3d11` | Direct3D 11 / RHI | fallback hardware Windows |
| `software` | Qt Quick software renderer | fallback CPU, senza GPU |

Esempi:

```sh
# macOS, bundle locale
./build/DecoDXLog.app/Contents/MacOS/DecoDXLog --graphics auto
./build/DecoDXLog.app/Contents/MacOS/DecoDXLog --graphics software

# Linux, anche con AppImage
./DecoDXLog-<version>-linux-x86_64.AppImage --graphics opengl
./DecoDXLog-<version>-linux-x86_64.AppImage --graphics software

# Windows
DecoDXLog.exe --graphics d3d12
DecoDXLog.exe --graphics d3d11
DecoDXLog.exe --graphics software
```

Sono disponibili anche gli alias `--disable-gpu`, `--software-renderer` e
`--safe-graphics`. Per gli script di avvio si può usare la variabile comune:

```sh
DECODXLOG_GRAPHICS_BACKEND=opengl ./DecoDXLog
DECODXLOG_GRAPHICS_BACKEND=software ./DecoDXLog
# Linux: Vulkan solo se il driver e' verificato
./DecoDXLog --graphics vulkan
# In alternativa, conserva un QSG_RHI_BACKEND=vulkan gia' impostato
QSG_RHI_BACKEND=vulkan DECODXLOG_ALLOW_VULKAN=1 ./DecoDXLog
```

Il programma scrive nel terminale l'API realmente attivata. Se il scene graph
non riesce a inizializzarsi, riapre automaticamente il programma con il backend
successivo: su Windows `D3D12 → D3D11 → software`, su Linux `OpenGL → software`,
su macOS `Metal → software`.

## Mappa compatibile Linux

Su Linux la mappa predefinita non istanzia `Canvas` di Qt Quick: alcuni driver
Mesa/KWin possono continuare a disegnare la normale interfaccia OpenGL, ma
perdere il compositing quando una Canvas diventa visibile. La mappa compatibile
mostra terre emerse vettoriali statiche, reticolo, locatori lavorati, spot,
stazione e direzione del rotore con normali item Qt Quick, senza FBO o texture
Canvas.

Per provare volontariamente la cartografia completa con coste e fascia notte:

```sh
./DecoDXLog-<version>-linux-x86_64.AppImage --map-renderer canvas
# equivalente per gli script:
DECODXLOG_MAP_RENDERER=canvas ./DecoDXLog-<version>-linux-x86_64.AppImage
```

`--map-renderer safe` (oppure `DECODXLOG_MAP_RENDERER=safe`) forza la mappa
compatibile su qualunque sistema operativo, ed e' scelta automaticamente anche
quando Qt Quick gira in modalita' software.
