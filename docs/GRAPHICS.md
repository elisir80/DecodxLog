# DecoDXLog — modalità di rendering

DecoDXLog usa le stesse modalità di avvio di Qt Quick già adottate da Decodium 4.
La scelta viene applicata prima della creazione di `QGuiApplication`, quindi vale
per tutte le finestre QML.

## Modalità disponibili

| Modalità | Backend | Uso |
| --- | --- | --- |
| `auto` | Metal su macOS, D3D12 su Windows, scelta Qt su Linux | avvio normale |
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
```

Il programma scrive nel terminale l'API realmente attivata. Se il scene graph
non riesce a inizializzarsi, riapre automaticamente il programma con il backend
successivo: su Windows `D3D12 → D3D11 → software`, su Linux `Vulkan/auto →
OpenGL → software`, su macOS `Metal → software`.
