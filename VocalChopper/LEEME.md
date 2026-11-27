# Vocal Chopper · Soundtastics — plugin

## Instalar en Windows (Ableton)
1. Abre `VocalChopper-Windows.zip`.
2. Copia la carpeta **`Vocal Chopper.vst3`** en `C:\Program Files\Common Files\VST3\`
   (Windows te pedirá permiso de administrador: acepta).
3. En Ableton: *Preferencias → Plug-ins* → activa "Usar carpeta del sistema VST3" y pulsa **Re-escanear**.
4. Lo encontrarás en el navegador, en *Plug-ins → Soundtastics → Vocal Chopper*. Arrástralo a una **pista MIDI**
   (es un instrumento, como Simpler).

`Vocal Chopper.exe` es la versión independiente (sin Ableton), útil para probar rápido.

## Instalar en Mac
1. Abre `VocalChopper-Mac.zip`.
2. Copia `Vocal Chopper.vst3` en `~/Librería/Audio/Plug-Ins/VST3/` y `Vocal Chopper.component` en
   `~/Librería/Audio/Plug-Ins/Components/`.
3. Como todavía no está firmado con un certificado de Apple, la primera vez abre la app *Terminal* y pega:
   `xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Vocal Chopper.vst3" ~/Library/Audio/Plug-Ins/Components/"Vocal Chopper.component"`

## Cómo se usa (igual que el prototipo)
- **Cargar audio**: arrastra un .wav/.mp3/.aiff/.flac desde el navegador de Ableton o el Explorador a la pantalla,
  usa el botón de importar (abajo a la izquierda) o elige un sonido de la **librería** (arriba a la derecha).
- **Tocar chops**: notas MIDI desde **C1** (nota 36) en adelante, clic en la onda o teclas A W S E D F T G…
- **Play**: arranca solo cuando le das al play de Ableton, sincronizado al compás. El botón Play del plugin
  también funciona por su cuenta.
- **Roll en vivo (Slip Mode)**: con el Play en marcha, mantén una nota/chop: hace roll a la subdivisión del mando SIZE
  y al soltar vuelve a su sitio. ∞ (Latch) deja el roll fijo.
- **Edición individual**: al tocar un chop queda seleccionado (aviso amarillo): PITCH y la dirección afectan solo a él.
  La × del aviso vuelve a la edición global.
- **Cortes**: doble clic en la onda añade un corte; doble clic en una línea lo borra; arrastra líneas y marcadores.
- **PITCH + Alt/Shift** = formantes. **Ctrl** al arrastrar = ajuste fino. **Doble clic** en un mando = reset.
- **RENDER FILE**: crea un WAV de 24 bits (2–8 compases) en `Documentos/Soundtastics/Vocal Chopper/Renders`.
  Cuando pone ARRÁSTRAME, arrástralo directamente a una pista de audio de Ableton (o haz clic para guardarlo donde quieras).
- **Grabar (●)**: graba lo que entra por la entrada de audio del plugin (en la versión independiente, el micrófono;
  allí hay que activar la entrada en *Options → Audio Settings* y desmarcar "Mute audio input").
- Todo (audio, cortes y mandos) se guarda con la sesión de Ableton.

## Qué falta respecto a la lista de "mejoras" del CLAUDE.md
- El stretch y el pitch usan el mismo motor por granos del prototipo; los formantes son una aproximación con filtros.
  El siguiente paso es meter un motor de calidad vocal (Signalsmith Stretch, licencia MIT, apto para vender).
- La librería sigue siendo de sonidos sintetizados de prueba: hay que sustituirlos por las grabaciones reales.
- El hueco del logo arriba a la izquierda tiene un icono provisional: falta el logo oficial en PNG/SVG.
- Para vender en Mac hará falta firmar y notarizar con una cuenta de Apple Developer.

## Volver a compilar
El código está en `Source/`. Cada vez que se sube un cambio a GitHub, la receta `.github/workflows/vocal-chopper.yml`
compila Windows y Mac en la nube y deja los ZIP en `VocalChopper/builds/`.
