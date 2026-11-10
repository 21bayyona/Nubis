# NUBIS — Granular Texture Engine (VST3)

Plugin granular híbrido (efecto + instrumento) escrito en C++17 con JUCE 8.
Transforma cualquier señal en nubes, pads, drones, texturas metálicas, glitches y paisajes cinematográficos.
Fabricante en el DAW: **21bayyona**.

Se compilan dos plugins con el mismo código:

| Plugin | Uso en FL Studio | Por defecto |
|---|---|---|
| **Nubis** (`Nubis.vst3`) | Slot de efecto del Mixer | Fuente = entrada en vivo, modo continuo |
| **Nubis Instrument** (`Nubis Instrument.vst3`) | Channel Rack (instrumento) | Fuente = interna, tocado por MIDI |

---

## 1. Compilar en Windows 10/11 (64 bits)

### Requisitos (una sola vez)
1. **Visual Studio 2022 Community** (gratis) con la carga de trabajo **"Desarrollo para el escritorio con C++"**.
2. **CMake ≥ 3.22** — https://cmake.org/download/ (marca "Add CMake to PATH").
3. **Git** — https://git-scm.com/download/win

### Compilar
Abre una terminal (cmd) en la carpeta del proyecto y ejecuta:

```bat
scripts\build_windows.bat
```

La primera vez descarga JUCE 8.0.4 automáticamente (≈150 MB) y tarda unos minutos. Al terminar tendrás:

```
build\Nubis_artefacts\Release\VST3\Nubis.vst3
build\NubisInstrument_artefacts\Release\VST3\Nubis Instrument.vst3
build\Nubis_artefacts\Release\Standalone\Nubis.exe      (versión independiente para probar sin DAW)
```

Comandos equivalentes a mano:

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

Opciones de CMake útiles: `-DNUBIS_JUCE_DIR=C:\ruta\a\JUCE` (usar un JUCE local en vez de descargarlo), `-DNUBIS_BUILD_INSTRUMENT=OFF`, `-DNUBIS_BUILD_STANDALONE=OFF`, `-DNUBIS_COPY_AFTER_BUILD=ON` (copia al sistema tras compilar; requiere terminal como administrador).

El runtime de MSVC va enlazado de forma estática: el `.vst3` funciona en otro PC sin instalar redistribuibles.

### Instalar
```bat
powershell -ExecutionPolicy Bypass -File scripts\install_windows.ps1
```
Copia los dos `.vst3` a `C:\Program Files\Common Files\VST3` (pide permisos de administrador).

### Abrir en FL Studio
1. `Options > Manage plugins` → **Find installed plugins** (o "Find more plugins").
2. **Efecto:** en el Mixer, slot vacío → `Nubis`.
3. **Instrumento:** Channel Rack → `+` → `Nubis Instrument`. Tócalo con el piano roll o un teclado MIDI.
4. Los parámetros se automatizan desde FL (clic derecho en un control del plugin → *Create automation clip*, o "Browse parameters").

> MIDI en la versión efecto: si quieres usar ENV en modo MIDI o el modo de juego MIDI dentro del Mixer, envía MIDI al plugin desde un canal con *MIDI Out* al puerto configurado en el wrapper del plugin. Para tocar con notas, lo más directo es usar **Nubis Instrument**.

---

## 2. Qué hace cada sección

### Fuente (cabecera del display central)
- **Live Input** – graba continuamente los últimos 12 s de la entrada en un buffer circular; los granos leen de ahí.
- **Sample** – carga un archivo (wav/aiff/flac/ogg/mp3, hasta 60 s). Arrastra el archivo al plugin o pulsa *LOAD SAMPLE*. La ruta se guarda con el proyecto.
- **Internal** – 4 fuentes sintetizadas internamente (Harmonic, Vox, Glass, Air), afinadas en Do4 (nota MIDI 60).
- **Play Mode** – *Continuous* (la nube suena siempre) o *MIDI* (las notas abren la nube, la transportan y ENV 1 hace de envolvente de amplitud; acordes polifónicos de hasta 8 notas, cada grano toma una nota en round-robin).

### GRAIN ENGINE
| Control | Efecto real |
|---|---|
| Size | 1–2000 ms de duración de grano |
| Density | granos por segundo (0.5–400) |
| Position | punto de lectura (en Live: 100 % = lo más reciente) |
| Pos Random | dispersión aleatoria de la posición por grano |
| Pitch / Pitch Random | ±24 st / dispersión aleatoria en semitonos |
| Jitter | aleatoriedad temporal del disparo (0 = periódico → tonos metálicos/robóticos) |
| Overlap | garantiza solapamiento: longitud mínima = 4 × intervalo entre granos |
| Spread / Pan | dispersión estéreo / centro de panorama |
| Reverse | probabilidad de grano invertido |
| Direction | velocidad de escaneo del cabezal (−2× … +2× tiempo real) |
| Envelope | Hann, Gauss, Triangle, Trapezoid, Perc, Swell, Tukey |
| Freeze | Live: deja de grabar (congela el buffer). Sample/Internal: detiene el escaneo |

Lectura con interpolación Hermite de 4 puntos, envolventes tabuladas con ceros exactos en los extremos y normalización de ganancia por solapamiento (1/√N) para que la nube no sature internamente.

### TEXTURE ENGINE
| Control | Implementación |
|---|---|
| Texture | dureza del borde de la envolvente (suave ↔ granuloso) |
| Smoothness | doble paso-bajo de 18 kHz a 900 Hz sobre la nube |
| Spectral Blur | STFT 2048/75 %: suavizado temporal de magnitudes + difusión de fase (rama seca alineada en latencia) |
| Randomness | variación por grano de amplitud, duración, forma de envolvente y dureza |
| Diffusion | red de 4 allpass por canal (difumina transitorios) |
| Density | granos acompañantes (micro-desafinados y a la octava) que engordan la nube |
| Width | anchura M/S 0–200 % |
| Feedback | Live: la nube se reinyecta en el buffer de captura (regeneración granular). Otras fuentes: línea de regeneración sincronizada al tamaño de grano. Saturación suave + bloqueo de DC |

### CREATIVE
- **Infinite Freeze** – congela el buffer en vivo con un crossfade de bucle sin clic **y guarda el audio congelado dentro del proyecto** (al reabrir la sesión sigue sonando aunque no entre audio).
- **Spectral Play** – cuantiza la altura de cada grano a una escala (Chromatic, Major, Minor, Pentatonic, 5ths+8ves) o la reparte por la serie armónica (Harmonics). *Spectral Amt* = probabilidad de cuantizar.
- **Chaos** – caminatas aleatorias suavizadas sobre posición, pitch, tamaño, densidad y panorama + saltos de octava e inversiones por grano.
- **Evolve / Evolve Rate** – deriva lenta y continua (osciladores de razones irracionales + deriva de posición) sobre posición, tamaño, blur, difusión y pitch random.
- **Reverse Cloud** – granos invertidos con envolvente Swell, más largos y dispersos.
- **Drone** – alarga y solapa los granos, ralentiza el escaneo, añade capas a −12/+7/+12/+19 st y difusión.
- **RANDOMIZE** – clic: aleatorio musical; Shift+clic: experimental. Tamaño y densidad se eligen juntos para que siempre haya solapamiento útil; feedback, drive y crush quedan limitados y el limitador sigue activo.

### MODULACIÓN (panel derecho)
- **4 LFO** (Sine, Triangle, Saw, Square, Random suave, S&H), rate libre 0.01–20 Hz o sincronizado al tempo (fase alineada a la posición del DAW).
- **2 ADSR** con disparo MIDI, Loop (auto-ciclo) o Audio (seguidor de envolvente de la entrada, útil en el Mixer).
- **Random** (rate + smooth) y **Step Sequencer** de 16 pasos (longitud 1–16, división de tempo, glide). Dibuja los pasos con el ratón.
- **4 Macros**. Cada preset de fábrica trae las 4 macros ya asignadas.
- **Arrastrar y soltar:** arrastra un chip (LFO 1… SEQ, M1…M4) sobre cualquier knob con anillo. El anillo ámbar muestra la modulación en tiempo real.
- **Clic derecho en un knob:** ajustar cantidad (±10/25/50/100 %), quitar, o "Modulate with…".
- **MATRIX:** 16 ranuras fuente → destino → cantidad (todas automatizables). 39 destinos: todos los de granos, textura, funciones creativas, efectos principales y Dry/Wet.

### EFECTOS (abajo)
Reverb FDN de 8 líneas · Delay estéreo/ping-pong sincronizable · Distortion 2× sobremuestreada (Soft/Hard/Fold/Tube) · Bitcrusher · Filtro SVF multimodo con drive · Chorus 3 voces · Phaser 6 etapas · Frequency Shifter (Hilbert, ~48 dB de rechazo de banda imagen) · Stereo Imager con mono-bass.
Cada uno con su botón de encendido (fundido de 15 ms, sin clics). **Arrastra los chips para cambiar el orden** o usa `<` `>`. El orden se guarda con el proyecto y con los presets.

### SALIDA
Input gain, Dry/Wet (equal-power), Output, **limitador** de seguridad a −0.3 dBFS con medidor de reducción. Con el limitador apagado sigue habiendo una protección de último recurso (clamp a ±4) y detección de NaN.

---

## 3. Presets de fábrica (32)

| Categoría | Presets |
|---|---|
| Atmospheric | Glass Horizon · Morning Haze · Aurora Veil |
| Experimental | Particle Collider · Quantum Foam · Inside Out · Spectral Lattice |
| Vocal Processing | Ghost Choir · Vocal Smear · Formant Robot |
| Granular Pads | Silk Pad · Velvet Strata · Halo Pad |
| Glitch | Stutter Grid · Data Shards · Tape Skip |
| Metallic | Iron Bells · Resonant Steel · Chrome Drift |
| Cinematic | Tension Rise · Monolith · Nebula Score |
| Drones | Endless Om · Deep Current · Solar Hum |
| Rhythmic | Pulse Engine · Grain Gate · Polymeter Dust |
| Dark Textures | Abyssal Bloom · Rust Choir · Night Engine · Black Ice |

Los presets no cambian la fuente ni el modo de juego (Live/Sample/Internal, Continuous/MIDI): así un preset nunca deja de procesar tu pista por sorpresa.
Los presets de usuario se guardan en `Documentos\21bayyona\Nubis\Presets\*.nubispreset` (SAVE / LOAD, o arrastra un `.nubispreset` al plugin).

---

## 4. Arquitectura

```
Source/
  dsp/                     núcleo DSP sin dependencias de JUCE (testeable aparte)
    DspUtils.h             suavizadores, SVF TPT, líneas de retardo, FFT, oversampler 2x, Hilbert, ring SPSC
    Grain.h/.cpp           voz de grano + tablas de envolvente
    GranularEngine.h/.cpp  buffer de captura, sample/internal, planificador, freeze, funciones creativas, etapa de textura
    SpectralBlur.h/.cpp    STFT blur
    ModulationEngine.h/.cpp LFO, ADSR, random, secuenciador, macros, matriz
    EffectsEngine.h/.cpp   9 efectos + routing reordenable
  Parameters.h/.cpp        ~180 parámetros automatizables (APVTS), destinos de modulación
  PresetManager.h/.cpp     presets de fábrica y de usuario
  FactoryPresets.cpp       los 32 presets
  PluginProcessor.h/.cpp   procesado, estado, carga de samples, randomize
  PluginEditor.h/.cpp      interfaz (superficie 1280×800 escalada por transformación)
  ui/                      look & feel, knobs, visualizador de granos, espectrograma, paneles
Resources/Fonts/           Outfit + Geist Mono (SIL Open Font License, incluidas)
Tests/                     tests DSP, test de host VST3, capturas de la UI
```

**Reglas de tiempo real que cumple el código**
- Ninguna reserva de memoria, lock bloqueante ni E/S en el hilo de audio. Todo se reserva en `prepareToPlay`.
- Parámetros leídos con atómicos; el sample cargado se entrega por intercambio de punteros lock-free y el buffer viejo se libera en el hilo de mensajes cuando ningún grano lo usa.
- Procesado en sub-bloques de 32 muestras (modulación a ~1.5 kHz), por lo que cualquier tamaño de buffer del host funciona (probado de 1 a 4096).
- Suavizado de todos los controles continuos; efectos con fundido al encender/apagar; bucles de freeze con crossfade equal-power.
- Sample rates 44.1–192 kHz; mono y estéreo (entrada y salida).
- Latencia reportada 0. La rama húmeda de Spectral Blur lleva 2048 muestras de retardo interno (la mezcla dentro del blur está alineada, así que no produce filtrado en peine).

---

## 5. Pruebas incluidas

```bat
build\Release\NubisDspTests.exe
```
11 pruebas del núcleo DSP: latencia del oversampler, rechazo de banda imagen del frequency shifter, transparencia del spectral blur, barrido de valores extremos, freeze y su restauración, fuentes internas + MIDI, intercambio de samples, modulación, efectos con órdenes aleatorios, y benchmark de CPU.

Test de host (carga el `.vst3` como lo haría un DAW):
```bat
cmake -S . -B build -DNUBIS_BUILD_HOST_TESTS=ON
cmake --build build --config Release --target NubisHostTest
build\NubisHostTest_artefacts\Release\NubisHostTest.exe "build\Nubis_artefacts\Release\VST3\Nubis.vst3"
```
Recorre los 32 presets (sin NaN, sin silencios, pico ≤ techo del limitador), buffers de 1–4096 muestras, sample rates de 44.1–192 kHz, automatización aleatoria de todos los parámetros, guardado/restauración de estado, persistencia del Infinite Freeze entre instancias y creación del editor.

---

## 6. Limitaciones conocidas (honestas)
- La cantidad de modulación se ajusta desde el menú de clic derecho del knob o desde la MATRIX; no hay "alt+arrastrar" sobre el knob.
- El orden de efectos se guarda con el proyecto y los presets, pero no es automatizable.
- Infinite Freeze necesita que haya entrado audio antes de activarlo (congela lo que hay en el buffer).
- El buffer en vivo es de 12 s; los samples se cargan hasta 60 s.
- El plugin no usa mensajes MIDI CC (solo notas), así que no expone parámetros MIDI-CC ocultos: la lista de automatización de FL muestra solo los parámetros reales.
