# Vocal Chopper — Soundtastics

Plugin VST3 (Windows/Mac) + AU (Mac) de chopeo de vocales y golpes con rolls/stutter.
La dueña del proyecto NO es programadora: explícale todo en castellano y "en cristiano",
paso a paso, sin dar por hecho que sabe qué es CMake, Git o un compilador.
Usa Ableton en Windows. Entrega siempre un ZIP descargable con lo que construyas.

## Qué hay en esta carpeta
- `docs/briefing.txt` — el briefing de producto original (requisitos funcionales).
- `prototipo/vocal-chopper.html` — prototipo funcional en navegador. ES LA REFERENCIA DE DISEÑO
  Y DE COMPORTAMIENTO: la interfaz del plugin debe verse y comportarse igual que este HTML.
  Ábrelo en Chrome para probarlo.

## Importante sobre el diseño
La interfaz es un diseño ORIGINAL de Soundtastics (cuerpo azul anodizado, mandos plateados,
pantalla de cristal negro, acento amarillo #ffd23f, cian #7fe6ff). No copiar la interfaz de
ningún otro plugin comercial (en concreto, no imitar "Mirage"). Arriba a la izquierda hay un
hueco reservado para el logo oficial de Soundtastics (fondo azul, símbolo blanco); el nombre
"soundtastics" solo se escribe con su lettering oficial, nunca con otra tipografía.

## Stack recomendado
- JUCE 8 (licencia gratuita JUCE Starter, ya usada en PITCHY) + CMake.
- Interfaz en gráficos nativos de JUCE (NO WebView): debe abrir al instante.
- Tamaño base 980×660, escalable.
- Formatos: VST3 + AU (+ Standalone para pruebas).

## Mapeo prototipo → plugin (valores ya ajustados en el HTML)
Todo el detalle está en el `<script>` del HTML; busca estas funciones:
- Detección de cortes: `analyse()` — modos AUTO / VOCAL (valles de envolvente, corte al final
  del silencio, snap a cruce por cero) / GOLPE (onsets por subida de energía, clavado al transiente).
  Chops mapeados cromáticamente desde C1 (nota MIDI 36, convención Ableton).
- Tempo: autocorrelación de onsets + ajuste fino por rejilla (`analyse`), Smart Tempo Fit
  = velocidad × BPM_host / BPM_loop (solo en modo GOLPE).
- Secuenciador: `doStep()` por semicorcheas sincronizado al BPM del host; flujo normal por granos
  (60 ms, hop 30 ms, ventana triangular); rolls como repeticiones con micro-fades 3–6 ms.
- Presets de capa base (`PRESETS`): STRAIGHT, STUTTER, BUILD-UP, TRIPLETS, GLITCH, CHOP SEQ.
- Slip Mode: `press()` / `release()` — quantize de entrada (OFF, 1/16, 1/8, 1/4); el cabezal
  sigue avanzando en segundo plano y vuelve a su sitio al soltar. Latch (∞) deja el roll fijo.
- Direcciones: FORWARD, REVERSE, PING-PONG, RANDOM; cada chop puede tener PITCH y dirección propios.
- Macros: STRETCH (×0.5–×2 de longitud), PITCH (±12 st, Alt/Shift = formantes), SIZE (izq. = chop
  entero / roll lento → dcha. = chop corto / roll granular: 1/4 … 1/64 con tresillos),
  ARTIFACTS (0–50 % drive de cinta + wow; 50–100 % bitcrusher + downsample + micro-glitches),
  BLUR (reverb + delay ping-pong 3/16 sincronizado + ensanche Haas, en paralelo).
- IN: −24…+12 dB con soft-clipper a partir de las 12 h. OUT con limitador de seguridad.
- Automáticos: ducking de la cola de BLUR, de-esser ligado al PITCH, limitador, FX Lock.
- Overlay de ajustes: detección, sensibilidad, filtro DJ (LPF/HPF en un mando), Key Lock
  (tónica + mayor/menor), Smart Tempo Fit, quantize de entrada, FX Lock.
- RENDER FILE: renderiza a WAV 24 bits y se arrastra directamente a una pista del DAW.

## Mejoras que el plugin real debe tener (el navegador no podía)
- Time-stretch y pitch-shift de calidad vocal con preservación de formantes de verdad
  (p. ej. Signalsmith Stretch o Rubber Band; comprobar la licencia antes de vender).
- Grabación desde micrófono / entrada del canal, render offline y drag & drop real al DAW.
- Librería de fábrica con grabaciones reales (en el prototipo son sonidos sintetizados de prueba).
- Estado completo guardado con la sesión del DAW (audio, cortes, mandos).
