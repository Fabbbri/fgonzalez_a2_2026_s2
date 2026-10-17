# Cuatro mundos. Una pregunta.

Montaje de divulgación original basado en el enunciado local
`ray tracing/docs/proyecto.md`: 4 minutos y 31 segundos, cuatro problemas, cinco modelos,
hallazgos y voces sugeridas para los cuatro integrantes. Incluye 26 escenas,
transiciones, guion sincronizado, subtítulos y gráficas animadas con resultados
reales del repositorio. DL se identifica como preliminar.

Desde la raíz del proyecto:

```bash
xdg-open video/animaciones.html
```

Funciona desde disco, sin instalación, sin servidor y sin conexión. Si ya estaba
abierto, recargá la página con Ctrl+Shift+R.

- **Reproducción:** pausa, reinicio, selección de escena, línea de tiempo y
  pantalla completa. Espacio pausa y las flechas cambian cinco segundos.
- **Guion:** el texto de la escena actual aparece debajo del video. El guion
  completo con dirección visual y reparto está en `guion.md`.
- **Storyboard:** desplegá las 26 escenas para saltar a una idea concreta.
- **Exportación:** WebM a 1920 × 1080, con tasa solicitada de 60 fps y 20 Mbps.
  Elegí el montaje completo o una escena (10 segundos; RAM, 20; despedida, 11). La exportación ocurre
  en tiempo real y no incluye audio ni los controles. Requiere un navegador
  compatible con MediaRecorder y captureStream, como Chrome o Firefox.
- **Subtítulos:** pueden incrustarse en el canvas o agregarse después desde
  `subtitulos.srt`. Desmarcá la casilla para exportar una versión limpia.
- **Narración:** graben las voces reales del grupo y agréguenlas en su editor;
  el guion distribuye las intervenciones entre cuatro personas. Ensayen y
  ajusten cada frase a sus diez segundos. No se genera narración automática.

Mantené la pestaña visible al exportar. La fluidez depende del navegador y del
hardware. Se puede terminar la exportación anticipadamente con el mismo botón.
OBS permite grabar la pantalla como alternativa. Si el sistema solicita movimiento
reducido, comienza pausada. La reproducción también se pausa al ocultar la pestaña.

Si tu editor requiere MP4 y tenés FFmpeg:

```bash
ffmpeg -i cuatro-problemas-completo-1080p60.webm -c:v libx264 -preset slow -crf 16 -pix_fmt yuv420p -r 60 -movflags +faststart cuatro-problemas-1080p60.mp4
```

## Archivos y edición

- `animaciones.html`: reproductor, controles y panel de narración.
- `animaciones.js`: dibujo vectorial Canvas, movimiento determinista por tiempo,
  reproducción y exportación.
- `guion.js`: fuente compartida de 26 escenas, narración, capítulos y resultados.
- `guion.md`: guion legible con tiempos, voces, dirección visual y fuentes.
- `subtitulos.srt`: subtítulos editables con los mismos tiempos del montaje.
- `generar-guion.cjs`: regenera Markdown y SRT desde la fuente compartida.

Después de editar el guion:

```bash
node video/generar-guion.cjs
```

Las barras usan resultados de `defensa/resultados/*/reporte.md`. Las galaxias,
redes, terrenos y diagramas de reparto son ilustraciones conceptuales, identificadas
como tales. No representan ejecución de los programas. Revisen las cifras si
actualizan los benchmarks; el video usa una instantánea de los reportes actuales.
El guion no atribuye a memoria o sincronización una causa demostrada por los datos.

Para previsualizar un instante concreto sin reproducción automática, agregá
`?t=166` al final de la URL del archivo (segundos entre 0 y 271).

## Escena de memoria y controles

Después de «Paralelizar también puede costar» se añade «La memoria también entra en juego» (3:30–3:50). Las tasas de RAM, 3 200 MT/s y 8 533 MT/s, fueron proporcionadas por el grupo. El flujo animado representa una hipótesis de suministro de datos, no mediciones de ancho de banda ni una causa demostrada. El guion y el SRT incluyen esta distinción.

Se quitaron la barra de progreso y el reloj del canvas exportado. La navegación temporal permanece en los controles del reproductor.

Verificación de esta actualización: 75 fotogramas ejecutados, continuidad de
las 26 escenas y sus subtítulos comprobada; escena de RAM revisada visualmente
en Chrome. La exportación completa de 4:20 no se volvió a ejecutar.

El cierre añade una tarjeta de «Gracias por ver» (4:20–4:31). Completa su
entrada y permanece visible al terminar; la última escena ya no se detiene
en mitad de un desvanecimiento. No lleva reloj ni barra de progreso incrustados.

La despedida reúne los cuatro temas en una composición de partículas, órbitas,
red neuronal, galaxia espiral, rayos y ondas de radar. La entrada del título
combina escala y desplazamiento suaves. Un velo oscuro conserva el contraste;
el movimiento se asienta antes del último fotograma para dejar un cierre completo.

La animación del cierre continúa durante los cinco segundos añadidos: las
partículas, órbitas, señales, rayos y radar se mantienen en movimiento durante
la despedida de 11 segundos y desaceleran justo antes del último fotograma.

Para 60 fps reales, volvé a exportar desde el HTML actualizado. El archivo
anterior no gana cuadros de animación al convertirlo a 60 fps. El navegador
solicita 60 cuadros por segundo y 20 Mbps; no se garantizan si el equipo o la
pantalla no sostienen esa cadencia. Mantené la ventana visible, con frecuencia
de pantalla de al menos 60 Hz, y cerrá tareas pesadas durante la exportación.
CRF 16 busca alta calidad visual; la conversión H.264 sigue siendo con pérdida.
