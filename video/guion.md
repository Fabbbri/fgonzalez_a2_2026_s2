# Cuatro mundos. Una pregunta.

Guion sincronizado con animaciones.html. Duración: **4 minutos y 31 segundos**, 26 escenas: 24 de 10 segundos, una comparación de RAM de 20 segundos y una despedida de 11 segundos. Aproximadamente **692 palabras**, 153 por minuto en promedio.

## Dirección y grabación

El relato abre con una pregunta, explica los modelos, visita cuatro problemas y termina contrastando resultados. Usar curiosidad, pausas y ejemplos visuales; el texto es original. Las escenas explican una idea a la vez y las barras muestran los datos reportados.

Reparto sugerido: Persona 1 presenta el contexto, modelos y DL (0:00–1:30); Persona 2, N-body (1:30–2:10); Persona 3, Ray Tracing (2:10–2:50); Persona 4, Sentinel y cierre (2:50–4:31). Cambien estas etiquetas por sus nombres. Todos deben participar, como pide el enunciado. Pueden añadir sus rostros desde el editor en un margen sin tapar las visualizaciones.

Ensayen cada bloque de 10 segundos y el de RAM de 20 segundos; la despedida dispone de 11 segundos; la duración de una lectura depende del hablante. Graben las frases por separado y alinéenlas con los tiempos de este documento. Los cambios de escena tienen transiciones breves. Si una frase necesita más tiempo, extiendan el plano en el editor. La animación exporta video **sin audio**.

El botón de subtítulos permite exportar una versión limpia y agregar el archivo SRT en el editor. La narración y el texto SRT salen de la misma fuente (guion.js). Para mayor legibilidad se pueden dividir las entradas de 10 segundos en el editor siguiendo el ritmo real de las voces.

## Montaje

### 0:00–0:10 · Cuatro mundos. Una pregunta.

**Voz:** Persona 1. **Tema:** Universos.

> ¿Qué tienen en común una galaxia, una imagen realista, una red neuronal y un satélite? Todos esconden trabajo que podemos repartir. Pero repartirlo tiene un costo.

**En pantalla:** Cuatro tarjetas aparecen con colores propios: red neuronal, galaxias, rayos y radar. La pregunta central surge al final.

### 0:10–0:20 · La promesa del paralelismo

**Voz:** Persona 1. **Tema:** Universos.

> Imaginemos una fila de tareas. Con un trabajador avanzamos de una en una. Con varios podemos avanzar en paralelo, siempre que las tareas lo permitan.

**En pantalla:** Una fila de tareas se convierte en cuatro filas. Mostrar que repartir requiere independencia; el ritmo dibujado no es una medición.

### 0:20–0:30 · Fine-grained

**Voz:** Persona 1. **Tema:** Modelos.

> En grano fino alternamos tareas en pequeños turnos. Nuestra planificación es cooperativa: intercalar tareas en un solo hilo no significa calcularlas simultáneamente en varios núcleos.

**En pantalla:** Diagrama temporal de cuatro tareas: solo una ocupa cada turno del único hilo real. Un cursor recorre los quantums.

### 0:30–0:40 · Coarse-grained

**Voz:** Persona 1. **Tema:** Modelos.

> En grano grueso usamos hilos reales que avanzan y pueden esperar. Modelamos bloqueos en varios problemas; el sistema operativo también decide cuándo ejecutar cada hilo.

**En pantalla:** Cuatro hilos avanzan con segmentos de espera. Las pausas explican el modelo; no equivalen a tiempos de los experimentos.

### 0:40–0:50 · CMP y SMT

**Voz:** Persona 1. **Tema:** Modelos.

> CMP distribuye trabajadores entre núcleos físicos. SMT aprovecha hilos lógicos que comparten un núcleo. Comparamos SMT activo e inactivo desde Linux.

**En pantalla:** Primero cuatro núcleos con un trabajador; después dos contextos lógicos por núcleo y recursos compartidos. Topología esquemática.

### 0:50–1:00 · De píxeles a una predicción

**Voz:** Persona 1. **Tema:** Deep Learning.

> Nuestro primer mundo es una red neuronal que reconoce dígitos. Los píxeles atraviesan capas de operaciones hasta producir una predicción. Esta es una representación conceptual.

**En pantalla:** Dígito ilustrativo, conexiones y señales recorriendo capas. La cantidad dibujada de nodos no representa la arquitectura completa.

### 1:00–1:10 · Aprender es corregir

**Voz:** Persona 1. **Tema:** Deep Learning.

> Durante el entrenamiento comparamos la salida con la respuesta esperada, propagamos el error y actualizamos los pesos. Repetir este ciclo permite ajustar el comportamiento de la red.

**En pantalla:** Tres tarjetas: predicción, error y actualización. Se destaca una por turno y una flecha devuelve al ciclo de entrenamiento.

### 1:10–1:20 · Repartir las actualizaciones

**Voz:** Persona 1. **Tema:** Deep Learning.

> En nuestra implementación, forward y backpropagation permanecen secuenciales; distribuimos las actualizaciones. Por eso sumar trabajadores no acelera todo el entrenamiento y la coordinación puede dominar.

**En pantalla:** Filas de una matriz se iluminan por bloques y se asignan a trabajadores. Se aclara qué parte del código se distribuye.

### 1:20–1:30 · Más trabajadores, peor tiempo

**Voz:** Persona 1. **Tema:** Deep Learning.

> Con CMP, cuatro trabajadores alcanzaron aproximadamente dos coma seis veces la velocidad secuencial. Con ocho, bajó a uno coma cinco. DL todavía tiene pocas repeticiones.

**En pantalla:** Barras CMP reales para P=1,2,4,8, con IC95 y aviso de campaña preliminar. Señalar el descenso de P=4 a P=8.

### 1:30–1:40 · Dos galaxias en interacción

**Voz:** Persona 2. **Tema:** N-body.

> Ahora cambiemos de escala. En N-body, cada cuerpo recibe la atracción de todos los demás. Dibujamos dos galaxias para explicar este cálculo, sin simular aquí su trayectoria.

**En pantalla:** Dos discos estelares con conexiones de fuerza para un cuerpo. El movimiento es ilustrativo, no una integración física.

### 1:40–1:50 · Todos influyen sobre todos

**Voz:** Persona 2. **Tema:** N-body.

> Para calcular la fuerza de un cuerpo recorremos los demás. Con este método directo, duplicar los cuerpos produce aproximadamente cuatro veces más interacciones por paso.

**En pantalla:** Un cuerpo central recibe contribuciones de 12 y después 24 vecinos. La anotación cambia de N² a aproximadamente 4N².

### 1:50–2:00 · Un paso, varias fases

**Voz:** Persona 2. **Tema:** N-body.

> Repartimos los cuerpos entre trabajadores. Las fases deben sincronizarse para que el siguiente cálculo use un estado consistente. El trabajador más lento puede hacer esperar a los demás.

**En pantalla:** Cuatro bloques calculan a ritmos distintos; los primeros esperan en una barrera hasta que termina el último.

### 2:00–2:10 · El máximo no estuvo en ocho

**Voz:** Persona 2. **Tema:** N-body.

> N-body alcanzó aproximadamente tres coma dieciséis veces la velocidad secuencial con cuatro trabajadores. Con ocho bajó a dos coma cincuenta y tres: más paralelismo no garantizó mejor rendimiento.

**En pantalla:** Barras CMP reales y sus IC95. El máximo observado está en cuatro trabajadores, no en ocho.

### 2:10–2:20 · Una imagen, millones de cálculos

**Voz:** Persona 3. **Tema:** Ray Tracing.

> En ray tracing seguimos rayos desde la cámara. Buscamos intersecciones y calculamos rebotes para construir el color de cada píxel. Cada región puede resolverse de forma independiente.

**En pantalla:** Un rayo sale de una cámara, intersecta una esfera y rebota. Un panel lateral representa el render dividido en regiones.

### 2:20–2:30 · Un píxel no es un solo rayo

**Voz:** Persona 3. **Tema:** Ray Tracing.

> Varias muestras por píxel y más rebotes aumentan el trabajo disponible. Para comparar modelos mantenemos la misma escena, resolución, semilla y calidad; así buscamos la misma imagen.

**En pantalla:** Ocho puntos de muestra emiten rayos desde un píxel. Una esfera y flechas de rebote explican el trabajo adicional.

### 2:30–2:40 · Repartir una imagen en tiles

**Voz:** Persona 3. **Tema:** Ray Tracing.

> Dividimos la imagen en tiles. Cuando un trabajador termina una región, toma otra. Este reparto ayuda cuando algunas zonas de la escena cuestan más que otras.

**En pantalla:** Una cuadrícula se completa por regiones de distintos colores, conectadas a cuatro trabajadores de una cola común.

### 2:40–2:50 · Aquí sí crece la aceleración

**Voz:** Persona 3. **Tema:** Ray Tracing.

> Con ocho trabajadores CMP, ray tracing logró aproximadamente cuatro coma setenta y ocho veces la velocidad secuencial. Fue una mejora clara para esta carga y este equipo.

**En pantalla:** Barras reales CMP: el speedup aumenta hasta 4.78× con ocho trabajadores. No presentar 8× como resultado.

### 2:50–3:00 · Dos observaciones del territorio

**Voz:** Persona 4. **Tema:** Sentinel-1.

> Nuestro cuarto mundo utiliza adquisiciones complejas de radar Sentinel-1. Cada píxel contiene amplitud y fase. Comparar dos adquisiciones permite construir un interferograma; aquí las imágenes son ilustrativas.

**En pantalla:** Un satélite cruza un terreno de curvas y lo ilumina con un haz. Ambas pasadas son ilustrativas; no usar como dato geográfico.

### 3:00–3:10 · Comparar fases

**Voz:** Persona 4. **Tema:** Sentinel-1.

> Multiplicamos una adquisición por el conjugado de la otra. La fase del producto expresa su diferencia de fase. Convertirla en desplazamiento requiere procesamiento e interpretación adicionales.

**En pantalla:** Dos vectores de fase giran en planos complejos. Se muestra A × B* y Δφ = φA − φB, sin inferir desplazamiento directamente.

### 3:10–3:20 · Muchos píxeles, poca aritmética

**Voz:** Persona 4. **Tema:** Sentinel-1.

> El producto se calcula píxel a píxel y se reparte en tiles. Pero tener muchos datos no basta: también importan el trabajo por píxel, la memoria y la planificación.

**En pantalla:** Cuadrícula de fases sintéticas y cuatro trabajadores: una operación compleja por píxel, partición por tiles.

### 3:20–3:30 · Paralelizar también puede costar

**Voz:** Persona 4. **Tema:** Sentinel-1.

> En esta carga, las configuraciones paralelas medidas de Sentinel no superaron la referencia secuencial. CMP con cuatro trabajadores quedó cerca de cero coma noventa y dos veces su velocidad.

**En pantalla:** Barras CMP reales, todas por debajo de la referencia de 1×. Señalar la base secuencial y los IC95.

### 3:30–3:50 · La memoria también entra en juego

**Voz:** Persona 4. **Tema:** Sentinel-1.

> Con RAM de tres mil doscientos megatransferencias por segundo, Sentinel no aceleró en el equipo de pruebas. En otro, con ocho mil quinientos treinta y tres, se observó mejora. Una mayor capacidad de mover datos podría ayudar a alimentar los núcleos. Pero también cambian CPU, caché y configuración: esta comparación no aísla la RAM.

**En pantalla:** Dos módulos DIMM con chips y contactos dorados: RAM reportada de 3 200 MT/s y 8 533 MT/s. Paquetes de datos alimentan cuatro núcleos; el flujo es conceptual. Primero se presenta la observación entre equipos, luego la hipótesis de memoria y sus factores de confusión. MT/s expresa megatransferencias por segundo; no se inventa un speedup del segundo equipo.

### 3:50–4:00 · Medir antes de concluir

**Voz:** Persona 4. **Tema:** Hallazgos.

> Ray tracing, N-body y Sentinel tienen doscientas repeticiones por configuración; DL tiene entre nueve y veinte. Calculamos intervalos de confianza y usamos perf para observar el proceso.

**En pantalla:** Tarjetas con 200, 9–20 e IC95; perf aparece con sus contadores y sus limitaciones. Evitar decir que DL tiene 200 muestras.

### 4:00–4:10 · La misma CPU, respuestas distintas

**Voz:** Persona 4. **Tema:** Hallazgos.

> Con cuatro trabajadores CMP, ray tracing y N-body aceleraron claramente; DL mejoró en su campaña preliminar y Sentinel no superó la base. El problema cambia el resultado.

**En pantalla:** Cuatro barras de speedup CMP con P=4 y línea de referencia de 1×. Cada problema usa su propia base secuencial.

### 4:10–4:20 · Más hilos no es una garantía

**Voz:** Persona 4. **Tema:** Hallazgos.

> La conclusión es simple: el paralelismo necesita trabajo independiente suficiente para compensar sus costos. Repartir, sincronizar y compartir recursos también consume tiempo. La respuesta está en medir.

**En pantalla:** Tres tarjetas: trabajo independiente, coordinación y recursos. Cierre con la frase «La respuesta está en medir».

### 4:20–4:31 · Gracias por ver

**Voz:** Persona 4. **Tema:** Cierre.

> Gracias por ver. Arquitectura de Computadores II.

**En pantalla:** El cierre aparece sobre una composición luminosa: constelación neuronal con señales, galaxia espiral, rayos reflejados y ondas de radar en las esquinas. Órbitas, partículas y brillos conectan los cuatro temas. El texto central entra con desplazamiento y escala suaves, protegido por un velo oscuro. Las órbitas, partículas, señales, rayos y radar continúan en movimiento durante los 11 segundos, desaceleran al final y «Gracias por ver» queda completamente visible. Bajo el título aparecen los créditos: Hecho por Jian Zheng, Fabricio Gonzalez, Melissa Vazquez y Jafet Diaz.

## Evidencia y alcance

- Enunciado: ray tracing/docs/proyecto.md (el archivo local se llama proyecto.md). Video solicitado: 3–5 minutos, público general, hallazgos y participación de todos.
- Datos: defensa/resultados/{dl,nbody,ray,sentinel}/reporte.md. Son una instantánea del repositorio; revisarlos antes de la entrega si se actualizan campañas.
- Se presenta CMP para facilitar una comparación con P constante. Los cinco modelos se explican; estas barras no resumen todas las políticas ni sustituyen el paper.
- Speedup = media secuencial / media del modelo; barras de incertidumbre = IC95 aproximado reportado. Cada problema tiene su propia referencia y carga.
- DL: CMP tiene 9–10 muestras por configuración y referencia de 20; el resto de la campaña tiene 10–20. No se afirma haber completado 200 en DL.
- SMT on/off se controla desde Linux. El diagrama de núcleos es conceptual; el equipo reporta 8 núcleos y 12 CPU lógicas con SMT.
- El movimiento galáctico, las franjas de radar, señales neuronales y tiempos de los diagramas son ilustrativos. No son resultados de una simulación ni evidencia de velocidad.
- Las causas de pérdidas de rendimiento requieren análisis: el video describe coordinación, memoria y recursos como factores posibles, sin atribuir una causa única a los contadores.
- La escena de RAM incorpora una observación informada por el grupo: equipo de las pruebas: 3 200 MT/s, segundo equipo: 8 533 MT/s y mejora en el segundo. Las tasas fueron proporcionadas por el grupo; no se midió el ancho de banda ni se controlaron canales, CPU o condiciones equivalentes. No es una comparación controlada ni demuestra que la RAM sea la causa del cambio de speedup.
- Marco técnico de la hipótesis: Intel Advisor, [Identify Performance Bottlenecks Using CPU Roofline](https://www.intel.com/content/www/us/en/docs/advisor/get-started-guide/2024-2/identify-bottlenecks-using-cpu-roofline.html). Distingue límites por ancho de banda y capacidad de cálculo; no demuestra que Sentinel esté limitado por memoria en estos equipos.

## Cambiar el guion

Editar guion.js modifica automáticamente el montaje y sus descargas. Regenerar este archivo y subtitulos.srt con:

```bash
node video/generar-guion.cjs
```
