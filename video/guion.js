/* Fuente única del montaje, narración, subtítulos y guion descargable.
 * Resultados transcritos de defensa/resultados/{problema}/reporte.md.
 * Los tiempos y las curvas mostrados son observaciones, no simulaciones.
 */
window.FILM = {
  shots: [
    ['intro','Cuatro mundos. Una pregunta.','¿Más hilos significa más velocidad?', '¿Qué tienen en común una galaxia, una imagen realista, una red neuronal y un satélite? Todos esconden trabajo que podemos repartir. Pero repartirlo tiene un costo.', 'Universos', 'Persona 1'],
    ['question','La promesa del paralelismo','Dividir el trabajo parece sencillo…', 'Imaginemos una fila de tareas. Con un trabajador avanzamos de una en una. Con varios podemos avanzar en paralelo, siempre que las tareas lo permitan.', 'Universos', 'Persona 1'],
    ['fine','Fine-grained','Cambiar de tarea en cada quantum', 'En grano fino alternamos tareas en pequeños turnos. Nuestra planificación es cooperativa: intercalar tareas en un solo hilo no significa calcularlas simultáneamente en varios núcleos.', 'Modelos', 'Persona 1'],
    ['coarse','Coarse-grained','Avanzar hasta un bloqueo', 'En grano grueso usamos hilos reales que avanzan y pueden esperar. Modelamos bloqueos en varios problemas; el sistema operativo también decide cuándo ejecutar cada hilo.', 'Modelos', 'Persona 1'],
    ['hardware','CMP y SMT','Núcleos distintos o recursos compartidos', 'CMP distribuye trabajadores entre núcleos físicos. SMT aprovecha hilos lógicos que comparten un núcleo. Comparamos SMT activo e inactivo desde Linux.', 'Modelos', 'Persona 1'],
    ['dl','De píxeles a una predicción','01 / Deep Learning', 'Nuestro primer mundo es una red neuronal que reconoce dígitos. Los píxeles atraviesan capas de operaciones hasta producir una predicción. Esta es una representación conceptual.', 'Deep Learning', 'Persona 1'],
    ['learn','Aprender es corregir','Predicción → error → actualización', 'Durante el entrenamiento comparamos la salida con la respuesta esperada, propagamos el error y actualizamos los pesos. Repetir este ciclo permite ajustar el comportamiento de la red.', 'Deep Learning', 'Persona 1'],
    ['matrix','Repartir las actualizaciones','El trabajo compartido también exige coordinación', 'En nuestra implementación, forward y backpropagation permanecen secuenciales; distribuimos las actualizaciones. Por eso sumar trabajadores no acelera todo el entrenamiento y la coordinación puede dominar.', 'Deep Learning', 'Persona 1'],
    ['dlResult','Más trabajadores, peor tiempo','Un resultado preliminar que merece atención', 'Con CMP, cuatro trabajadores alcanzaron aproximadamente dos coma seis veces la velocidad secuencial. Con ocho, bajó a uno coma cinco. DL todavía tiene pocas repeticiones.', 'Deep Learning', 'Persona 1'],
    ['nbody','Dos galaxias en interacción','02 / Simulación N-body', 'Ahora cambiemos de escala. En N-body, cada cuerpo recibe la atracción de todos los demás. Dibujamos dos galaxias para explicar este cálculo, sin simular aquí su trayectoria.', 'N-body', 'Persona 2'],
    ['gravity','Todos influyen sobre todos','Más cuerpos → muchas más interacciones', 'Para calcular la fuerza de un cuerpo recorremos los demás. Con este método directo, duplicar los cuerpos produce aproximadamente cuatro veces más interacciones por paso.', 'N-body', 'Persona 2'],
    ['barrier','Un paso, varias fases','Calcular → sincronizar → avanzar', 'Repartimos los cuerpos entre trabajadores. Las fases deben sincronizarse para que el siguiente cálculo use un estado consistente. El trabajador más lento puede hacer esperar a los demás.', 'N-body', 'Persona 2'],
    ['nbodyResult','El máximo no estuvo en ocho','Escalabilidad observada con CMP', 'N-body alcanzó aproximadamente tres coma dieciséis veces la velocidad secuencial con cuatro trabajadores. Con ocho bajó a dos coma cincuenta y tres: más paralelismo no garantizó mejor rendimiento.', 'N-body', 'Persona 2'],
    ['ray','Una imagen, millones de cálculos','03 / Ray Tracing', 'En ray tracing seguimos rayos desde la cámara. Buscamos intersecciones y calculamos rebotes para construir el color de cada píxel. Cada región puede resolverse de forma independiente.', 'Ray Tracing', 'Persona 3'],
    ['samples','Un píxel no es un solo rayo','Muestras y rebotes aumentan el trabajo', 'Varias muestras por píxel y más rebotes aumentan el trabajo disponible. Para comparar modelos mantenemos la misma escena, resolución, semilla y calidad; así buscamos la misma imagen.', 'Ray Tracing', 'Persona 3'],
    ['tiles','Repartir una imagen en tiles','Una cola de regiones independientes', 'Dividimos la imagen en tiles. Cuando un trabajador termina una región, toma otra. Este reparto ayuda cuando algunas zonas de la escena cuestan más que otras.', 'Ray Tracing', 'Persona 3'],
    ['rayResult','Aquí sí crece la aceleración','640 × 480 · 8 muestras · 12 rebotes', 'Con ocho trabajadores CMP, ray tracing logró aproximadamente cuatro coma setenta y ocho veces la velocidad secuencial. Fue una mejora clara para esta carga y este equipo.', 'Ray Tracing', 'Persona 3'],
    ['radar','Dos observaciones del territorio','04 / Sentinel-1 SLC', 'Nuestro cuarto mundo utiliza adquisiciones complejas de radar Sentinel-1. Cada píxel contiene amplitud y fase. Comparar dos adquisiciones permite construir un interferograma; aquí las imágenes son ilustrativas.', 'Sentinel-1', 'Persona 4'],
    ['phase','Comparar fases','A × conjugado(B)', 'Multiplicamos una adquisición por el conjugado de la otra. La fase del producto expresa su diferencia de fase. Convertirla en desplazamiento requiere procesamiento e interpretación adicionales.', 'Sentinel-1', 'Persona 4'],
    ['sentinelTiles','Muchos píxeles, poca aritmética','Partición espacial del interferograma', 'El producto se calcula píxel a píxel y se reparte en tiles. Pero tener muchos datos no basta: también importan el trabajo por píxel, la memoria y la planificación.', 'Sentinel-1', 'Persona 4'],
    ['sentinelResult','Paralelizar también puede costar','La referencia secuencial fue más rápida', 'En esta carga, las configuraciones paralelas medidas de Sentinel no superaron la referencia secuencial. CMP con cuatro trabajadores quedó cerca de cero coma noventa y dos veces su velocidad.', 'Sentinel-1', 'Persona 4'],
    ['memory','La memoria también entra en juego','Mismo problema, dos equipos y resultados distintos', ['Con RAM de tres mil doscientos megatransferencias por segundo, Sentinel no aceleró en el equipo de pruebas. En otro, con ocho mil quinientos treinta y tres, se observó mejora.', 'Una mayor capacidad de mover datos podría ayudar a alimentar los núcleos. Pero también cambian CPU, caché y configuración: esta comparación no aísla la RAM.'], 'Sentinel-1', 'Persona 4', 20],
    ['evidence','Medir antes de concluir','Repeticiones, incertidumbre y perf', 'Ray tracing, N-body y Sentinel tienen doscientas repeticiones por configuración; DL tiene entre nueve y veinte. Calculamos intervalos de confianza y usamos perf para observar el proceso.', 'Hallazgos', 'Persona 4'],
    ['compare','La misma CPU, respuestas distintas','Comparación CMP con cuatro trabajadores', 'Con cuatro trabajadores CMP, ray tracing y N-body aceleraron claramente; DL mejoró en su campaña preliminar y Sentinel no superó la base. El problema cambia el resultado.', 'Hallazgos', 'Persona 4'],
    ['ending','Más hilos no es una garantía','El mejor modelo depende del trabajo', 'La conclusión es simple: el paralelismo necesita trabajo independiente suficiente para compensar sus costos. Repartir, sincronizar y compartir recursos también consume tiempo. La respuesta está en medir.', 'Hallazgos', 'Persona 4'],
    ['thanks','Gracias por ver','Cuatro mundos. Una pregunta.', 'Gracias por ver. Arquitectura de Computadores II.', 'Cierre', 'Persona 4', 11]
  ].map(a=>({kind:a[0],title:a[1],subtitle:a[2],narration:Array.isArray(a[3])?a[3].join(' '):a[3],captions:Array.isArray(a[3])?a[3]:[a[3]],chapter:a[4],speaker:a[5],duration:a[6]||10})),
  results: {
    dl:{name:'Deep Learning',values:[1.0837,1.8387,2.6102,1.5480],ci:[.0394,.0606,.0823,.0514],n:'n = 9–10 / CMP · referencia n = 20 · preliminar',source:'defensa/resultados/dl/reporte.md'},
    nbody:{name:'N-body',values:[.9577,1.7609,3.1640,2.5319],ci:[.0039,.0105,.0183,.0471],n:'n = 200 por configuración · IC95 aproximado',source:'defensa/resultados/nbody/reporte.md'},
    ray:{name:'Ray Tracing',values:[1.0145,1.9661,3.2822,4.7811],ci:[.0033,.0041,.0090,.0135],n:'n = 200 por configuración · IC95 aproximado',source:'defensa/resultados/ray/reporte.md'},
    sentinel:{name:'Sentinel-1',values:[.6907,.8615,.9210,.9045],ci:[.0051,.0108,.0072,.0063],n:'n = 200 por configuración · IC95 aproximado',source:'defensa/resultados/sentinel/reporte.md'}
  }
};
// Acumular duraciones permite extender una escena sin desincronizar el montaje.
window.FILM.duration = window.FILM.shots.reduce((time,shot)=>{
  shot.start=time;
  shot.end=time+shot.duration;
  shot.captions=shot.captions.map((text,i,all)=>({text,start:time+i*shot.duration/all.length,end:time+(i+1)*shot.duration/all.length}));
  return shot.end;
},0);
