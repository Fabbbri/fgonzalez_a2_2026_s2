/* Framework experimental de modelos de ejecución Multithreading. Sin dependencias. */
(() => {
  "use strict";

  const $ = (id) => document.getElementById(id);
  const problems = {
    dl: {
      name: "Deep Learning", title: ["Deep", "Learning"],
      category: "APRENDIZAJE AUTOMÁTICO",
      description: "Una red neuronal aprende a reconocer dígitos. Medimos el costo de distribuir su entrenamiento.",
      chips: ["MNIST", "1 000 épocas", "100 muestras", "η = 0.1", "Semilla 1"],
      sceneLabel: "784 → 128 → 64 → 10", sceneCaption: "PROPAGACIÓN DE SEÑAL",
      sceneAlt: "Ilustración de una red neuronal con señales propagándose entre cuatro capas",
    },
    nbody: {
      name: "N-body Sim", title: ["Dos galaxias.", "Un sistema"],
      category: "INTERACCIÓN GRAVITACIONAL",
      description: "Cada cuerpo siente la atracción de todos los demás. Dos galaxias ponen a prueba el cálculo de fuerzas y su sincronización.",
      chips: ["1 000 cuerpos", "1 000 pasos", "Δt = 0.002", "O(N²)", "Semilla 42"],
      sceneLabel: "G = 1.0 / ε = 0.05", sceneCaption: "DOS DISCOS EN INTERACCIÓN",
      sceneAlt: "Ilustración de dos galaxias espirales en interacción gravitacional",
    },
    ray: {
      name: "Ray Tracing", title: ["Seguir", "la luz"],
      category: "TRAZADO DE RAYOS EN CPU",
      description: "De la cámara a la escena, rebote a rebote. El mismo render se reparte en tiles para comparar cinco formas de ejecutar.",
      chips: ["640 × 480 px", "Tiles 32 × 32", "8 muestras / px", "12 rebotes", "Semilla 2026"],
      sceneLabel: "CÁMARA → SUPERFICIE → REBOTE", sceneCaption: "TRANSPORTE DE LUZ",
      sceneAlt: "Ilustración del trazado de rayos sobre esferas con una superficie reflectante",
    },
    sentinel: {
      name: "Sentinel-1 SLC", title: ["Leer", "el territorio"],
      category: "INTERFEROMETRÍA RADAR",
      description: "Dos adquisiciones de radar de la misma región. Su producto complejo conjugado revela diferencias de fase, píxel a píxel.",
      chips: ["Sentinel-1 SLC", "24 444 × 1 499 px", "A · conjugado(B)", "O(N)"],
      sceneLabel: "RADAR DE APERTURA SINTÉTICA / SLC", sceneCaption: "OBSERVACIÓN DEL TERRENO",
      sceneAlt: "Ilustración de un satélite de radar escaneando un terreno representado por curvas de nivel",
    },
  };
  const chartDefinitions = [
    {file: "comparacion-eficiencia.png", title: "Eficiencia paralela", subtitle: "Qué aporta cada trabajador", caption: "Eficiencia = speedup / trabajadores. Permite observar cuánto rendimiento se obtiene por trabajador asignado."},
    {file: "comparacion-speedup.png", title: "Speedup", subtitle: "La aceleración frente a secuencial", caption: "Speedup = media secuencial / media de la configuración. Por encima de 1×, la ejecución es más rápida que la referencia."},
    {file: "comparacion-tiempo.png", title: "Tiempo de ejecución", subtitle: "El costo real de resolver el problema", caption: "Tiempos originales de las configuraciones, con sus intervalos de confianza. Menos tiempo es mejor; consulta la unidad del eje."},
    {file: "escalabilidad.png", title: "Escalabilidad", subtitle: "Qué cambia al sumar trabajadores", caption: "Evolución del speedup al variar el número de trabajadores, manteniendo la carga y distinguiendo las políticas de ejecución."},
  ];
  const keys = Object.keys(problems);
  const modelNames = {sequential: "Secuencial", fine: "Fine", coarse: "Coarse", cmp: "CMP", smt: "SMT"};
  const formatNumber = new Intl.NumberFormat("es-CR", {minimumFractionDigits: 2, maximumFractionDigits: 2});
  const motionPreference = window.matchMedia("(prefers-reduced-motion: reduce)");
  let currentKey = "dl";
  let sentinelEquipmentB = false;
  const selectedData = (key = currentKey) => window.METRICAS[
    key === "sentinel" && sentinelEquipmentB ? "sentinel_equipo_b" : key
  ];
  let currentFilter = "all";
  let currentChart = 0;
  let manuallyPaused = motionPreference.matches;
  let lastDialogTrigger = null;

  const modelOf = (row) => row[0].split(" · ")[0];
  const valueOf = (cell) => Number.parseFloat(cell);
  const imagePath = (index) => `${selectedData().folder}/${chartDefinitions[index].file}`;
  function make(tag, className, text) {
    const node = document.createElement(tag);
    if (className) node.className = className;
    if (text !== undefined) node.textContent = text;
    return node;
  }
  function shortConfig(row) {
    const parts = row[0].split(" · ");
    const smt = row[0].match(/SMT=(\d)/);
    return `${modelNames[parts[0]] || parts[0]} · ${parts[1]}${smt && parts[0] === "smt" ? ` · SMT ${smt[1] === "1" ? "on" : "off"}` : ""}`;
  }
  function setStat(id, value, unit) {
    const node = $(id);
    node.replaceChildren(document.createTextNode(value));
    if (unit) node.append(make("small", "", unit));
  }
  function workloadChips(key, data) {
    const values = Object.fromEntries(data.workload.split(", ").map((part) => part.split("=")).filter((pair) => pair.length === 2));
    const number = (field) => new Intl.NumberFormat("es-CR").format(Number(values[field]));
    const chips = {
      dl: () => ["MNIST", `${number("epocas")} épocas`, `${number("muestras")} muestras`, `η = ${values.learning_rate}`, `Semilla ${values.semilla}`],
      nbody: () => [`${number("cuerpos")} cuerpos`, `${number("pasos")} pasos`, `Δt = ${values.dt}`, "O(N²)", `Semilla ${values.semilla}`],
      ray: () => [`${number("ancho")} × ${number("alto")} px`, `Tiles ${values.tile_ancho} × ${values.tile_alto}`, `${values.muestras} muestras / px`, `${values.rebotes} rebotes`, `Semilla ${values.semilla}`],
      sentinel: () => ["Sentinel-1 SLC", `${number("ancho")} × ${number("alto")} px`, "A · conjugado(B)", "O(N)"],
    }[key]();
    return chips.some((chip) => /NaN|undefined/.test(chip)) ? problems[key].chips : chips;
  }

  function renderTable() {
    const data = selectedData();
    const rows = data.rows.filter((row) => currentFilter === "all" || modelOf(row) === currentFilter);
    const fastest = data.rows.reduce((a, b) => valueOf(a[3]) <= valueOf(b[3]) ? a : b);
    const body = $("results-body");
    body.replaceChildren();
    for (const row of rows) {
      const tr = make("tr", modelOf(row) === "sequential" ? "is-baseline" : row === fastest ? "is-best" : "");
      const config = make("td");
      const parts = row[0].split(" · ");
      config.append(make("span", "model-name", `${parts[0]} · ${parts[1]}`));
      if (row === fastest) config.append(make("span", "best-marker", "↗ mejor tiempo"));
      config.append(make("span", "config-detail", parts.slice(2).join(" · ")));
      tr.append(config);
      row.slice(1).forEach((cell, index) => {
        const td = make("td");
        if (index === 1) {
          const partial = Number.isFinite(Number(cell)) ? Number(cell) < 200 : !/alcanzada/i.test(cell);
          td.append(make("span", `reference${partial ? " partial" : ""}`, cell));
        } else td.textContent = cell;
        tr.append(td);
      });
      body.append(tr);
    }
    $("row-count").textContent = `${rows.length} de ${data.rows.length} configuraciones`;
    document.querySelectorAll("[data-filter]").forEach((button) => button.setAttribute("aria-pressed", String(button.dataset.filter === currentFilter)));
    document.querySelector('[data-filter="all"] span').textContent = data.rows.length;
  }

  function renderCharts() {
    const grid = $("charts");
    grid.replaceChildren();
    chartDefinitions.forEach((chart, index) => {
      const figure = make("figure", "chart");
      const header = make("div", "chart-header");
      const heading = make("div");
      heading.append(make("h4", "", chart.title), make("p", "", chart.subtitle));
      header.append(heading);
      const button = make("button", "chart-open");
      button.type = "button";
      button.setAttribute("aria-label", `Ampliar ${chart.title.toLowerCase()} de ${problems[currentKey].name}`);
      button.setAttribute("aria-haspopup", "dialog");
      const img = make("img");
      img.src = imagePath(index);
      img.alt = `${chart.title} — ${problems[currentKey].name}. Gráfica original del reporte de comparación.`;
      img.loading = "lazy";
      img.decoding = "async";
      const zoom = make("span", "chart-zoom");
      zoom.setAttribute("aria-hidden", "true");
      zoom.innerHTML = '<svg viewBox="0 0 20 20"><path d="M7 3H3v4m10-4h4v4M3 13v4h4m10-4v4h-4M3 3l5 5m9-5-5 5M3 17l5-5m9 5-5-5"/></svg>';
      button.append(img, zoom);
      button.addEventListener("click", () => openChart(index, button));
      figure.append(header, button, make("figcaption", "", chart.caption));
      grid.append(figure);
    });
  }

  function selectProblem(key) {
    if (!problems[key]) key = "dl";
    currentKey = key;
    currentFilter = "all";
    const config = problems[key];
    const data = selectedData(key);
    $("sentinel-equipment").hidden = key !== "sentinel";
    $("equipment-label").textContent = sentinelEquipmentB
      ? "Equipo B · memoria de 8533 MT/s · 8 núcleos / 8 CPU lógicas · SMT no soportado"
      : "Equipo A · memoria de 3200 MT/s · 8 núcleos / 12 CPU lógicas con SMT activo";
    $("equipment-toggle").textContent = sentinelEquipmentB
      ? "Regresar a los datos del equipo A · memoria de 3200 MT/s"
      : "Datos con un equipo diferente · memoria de 8533 MT/s";
    const article = $("problem-content");
    article.dataset.theme = key;
    $("ambient-scene").dataset.theme = key;
    $("problem-category").textContent = config.category;
    const title = $("problem-title");
    title.replaceChildren(document.createTextNode(config.title[0]), document.createElement("br"), document.createTextNode(config.title[1]), make("span", "", "."));
    $("problem-description").textContent = config.description;
    $("workload").replaceChildren(...workloadChips(key, data).map((text) => make("span", "", text)));
    $("scene-label").textContent = config.sceneLabel;
    $("scene-caption").textContent = config.sceneCaption;
    $("scene").setAttribute("aria-label", config.sceneAlt);
    $("report-link").href = `${data.folder}/reporte.md`;
    document.title = `${config.name} · Framework experimental de modelos de ejecución Multithreading`;
    document.querySelectorAll("[data-problem]").forEach((link) => {
      if (link.dataset.problem === key) link.setAttribute("aria-current", "page");
      else link.removeAttribute("aria-current");
    });
    const fastest = data.rows.reduce((a, b) => valueOf(a[3]) <= valueOf(b[3]) ? a : b);
    const bestSpeedup = data.rows.reduce((a, b) => valueOf(a[5]) >= valueOf(b[5]) ? a : b);
    setStat("best-time", formatNumber.format(valueOf(fastest[3])), "ms");
    $("best-config").textContent = shortConfig(fastest);
    setStat("best-speedup", formatNumber.format(valueOf(bestSpeedup[5])), "×");
    $("speedup-config").textContent = shortConfig(bestSpeedup);
    const counts = data.rows.map((row) => Number(row[1]));
    const min = Math.min(...counts), max = Math.max(...counts);
    setStat("sample-count", min === max ? String(min) : `${min}–${max}`, "n");
    $("sample-detail").textContent = `${data.rows.length} configuraciones · objetivo n = 200`;
    const referenceIsCount = data.headers[2] === "n referencia";
    $("reference-note").textContent = referenceIsCount
      ? `Objetivo: 200 repeticiones. La fuente denomina la tercera columna «n referencia»; se conserva su cantidad real (${[...new Set(data.rows.map((row) => row[2]))].join(", ")}).${min < 200 ? " Esta campaña contiene configuraciones con menos de 200 muestras." : ""}`
      : "Referencia 200: cumplimiento de las 200 repeticiones indicado en el reporte. Se conservan los valores originales y su precisión.";
    const next = keys[(keys.indexOf(key) + 1) % keys.length];
    const nextLink = $("next-link");
    nextLink.href = `#${next}`;
    nextLink.replaceChildren(make("b", "", problems[next].name), make("span", "", "→"));
    renderTable();
    renderCharts();
    scene.draw();
    atmosphere.draw();
  }

  function updateDialog(index) {
    currentChart = (index + chartDefinitions.length) % chartDefinitions.length;
    const chart = chartDefinitions[currentChart];
    $("dialog-counter").textContent = `${problems[currentKey].name} · ${currentChart + 1} de 4`;
    $("dialog-title").textContent = chart.title;
    $("dialog-image").src = imagePath(currentChart);
    $("dialog-image").alt = `${chart.title} de ${problems[currentKey].name}`;
    $("dialog-caption").textContent = chart.caption;
    $("original-image").href = imagePath(currentChart);
  }
  function openChart(index, trigger) {
    lastDialogTrigger = trigger;
    updateDialog(index);
    $("chart-dialog").showModal();
    document.body.style.overflow = "hidden";
    $("close-dialog").focus();
  }
  $("close-dialog").addEventListener("click", () => $("chart-dialog").close());
  $("prev-chart").addEventListener("click", () => updateDialog(currentChart - 1));
  $("next-chart").addEventListener("click", () => updateDialog(currentChart + 1));
  $("chart-dialog").addEventListener("click", (event) => {
    if (event.target === $("chart-dialog")) {
      const rect = event.target.getBoundingClientRect();
      if (event.clientX < rect.left || event.clientX > rect.right || event.clientY < rect.top || event.clientY > rect.bottom) event.target.close();
    }
  });
  $("chart-dialog").addEventListener("close", () => {
    document.body.style.overflow = "";
    if (lastDialogTrigger?.isConnected) lastDialogTrigger.focus({preventScroll: true});
  });
  document.addEventListener("keydown", (event) => {
    if (!$("chart-dialog").open) return;
    if (event.key === "ArrowLeft" || event.key === "ArrowRight") {
      event.preventDefault();
      updateDialog(currentChart + (event.key === "ArrowRight" ? 1 : -1));
    }
  });
  document.querySelectorAll("[data-filter]").forEach((button) => {
    button.addEventListener("click", () => { currentFilter = button.dataset.filter; renderTable(); });
  });
  $("download-csv").addEventListener("click", () => {
    const data = selectedData();
    const rows = data.rows.filter((row) => currentFilter === "all" || modelOf(row) === currentFilter);
    const csv = [data.headers, ...rows].map((row) => row.map((cell) => `"${cell.replaceAll('"', '""')}"`).join(",")).join("\r\n");
    const url = URL.createObjectURL(new Blob(["\ufeff", csv], {type: "text/csv;charset=utf-8"}));
    const anchor = make("a");
    anchor.href = url;
    anchor.download = `${currentKey}-comparacion${currentFilter === "all" ? "" : `-${currentFilter}`}.csv`;
    anchor.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  });

  /* Escenas ilustrativas: no se utilizan para producir ni representar métricas. */
  const scene = (() => {
    const canvas = $("scene"), ctx = canvas.getContext("2d");
    let width = 600, height = 370, time = 0, frame = 0, lastTimestamp = 0, visible = true;
    let randomState = 2026;
    const random = () => { randomState = (randomState * 1664525 + 1013904223) >>> 0; return randomState / 4294967296; };
    const stars = Array.from({length: 130}, () => [random(), random(), random()]);
    const particles = Array.from({length: 1100}, () => [random(), random(), random()]);
    const terrain = (x, y) => Math.sin(x * .027 + y * .011) * 17 + Math.cos(y * .032 - x * .013) * 13 + Math.sin((x + y) * .015) * 11;
    function line(points, color, lineWidth = 1) {
      ctx.strokeStyle = color; ctx.lineWidth = lineWidth; ctx.beginPath();
      points.forEach(([x, y], i) => i ? ctx.lineTo(x, y) : ctx.moveTo(x, y));
      ctx.stroke();
    }
    function dot(x, y, radius, color) {
      ctx.fillStyle = color; ctx.beginPath(); ctx.arc(x, y, radius, 0, Math.PI * 2); ctx.fill();
    }
    function background() {
      ctx.clearRect(0, 0, width, height);
      ctx.fillStyle = "#101b25"; ctx.fillRect(0, 0, width, height);
      ctx.strokeStyle = "#20303b"; ctx.lineWidth = .5;
      for (let x = 0; x < width; x += 40) { ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke(); }
      for (let y = 0; y < height; y += 40) { ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke(); }
      for (const [x, y, size] of stars) dot(x * width, y * height, size * .9 + .2, `rgba(164,186,199,${size * .26 + .1})`);
    }
    function network() {
      const counts = [7, 6, 5, 4];
      const left = width * .13, right = width * .88, center = height * .49;
      const spacing = Math.min(height * .065, 27);
      const layers = counts.map((n, layer) => Array.from({length: n}, (_, i) => [left + (right - left) * layer / 3, center + (i - (n - 1) / 2) * spacing]));
      const phase = (time * .38) % 4;
      for (let l = 0; l < 3; l++) {
        layers[l].forEach((a, i) => layers[l + 1].forEach((b, j) => {
          line([a, b], (i + j) % 3 === 0 ? "#94765d66" : "#41607140", .7);
          const progress = phase - l;
          if (progress >= 0 && progress <= 1 && (i + j) % 3 === 0) {
            const p = (progress + ((i + j) % 4) * .06) % 1;
            const x = a[0] + (b[0] - a[0]) * p, y = a[1] + (b[1] - a[1]) * p;
            dot(x, y, 2.1, "#ecad7e");
          }
        }));
      }
      layers.forEach((nodes, l) => {
        nodes.forEach(([x, y], i) => {
          const active = Math.abs(phase - l) < .45;
          dot(x, y, 7, "#111f2a");
          ctx.strokeStyle = active ? "#edb589" : "#9e8a71";
          ctx.lineWidth = active ? 1.4 : .8; ctx.beginPath(); ctx.arc(x, y, 7, 0, Math.PI * 2); ctx.stroke();
          dot(x, y, 2, active ? "#eab083" : "#72838b");
          if (active && i % 2 === 0) {ctx.strokeStyle = "#d38c5933"; ctx.beginPath(); ctx.arc(x, y, 13, 0, Math.PI * 2); ctx.stroke();}
        });
        ctx.fillStyle = "#93a1a8"; ctx.font = "9px monospace"; ctx.textAlign = "center";
        ctx.fillText(["ENTRADA", "OCULTA 01", "OCULTA 02", "SALIDA"][l], nodes[0][0], height * .85);
        ctx.fillStyle = "#dfac87"; ctx.fillText(["784", "128", "64", "10"][l], nodes[0][0], height * .85 + 16);
      });
    }
    function galaxies() {
      const scale = Math.min(width / 600, height / 365);
      const rotation = time * .055;
      const distance = 109 + Math.sin(time * .1) * 10;
      const centers = [[width * .5 - distance * scale, height * .48 + 22 * scale], [width * .5 + distance * scale, height * .48 - 18 * scale]];
      centers.forEach(([cx, cy], galaxy) => {
        ctx.save(); ctx.translate(cx, cy); ctx.rotate(galaxy ? -.52 : .37);
        const glow = ctx.createRadialGradient(0, 0, 1, 0, 0, 118 * scale);
        glow.addColorStop(0, galaxy ? "#d3a57455" : "#9cb6e166"); glow.addColorStop(.18, galaxy ? "#bf875d20" : "#739cd422"); glow.addColorStop(1, "#101b2500");
        ctx.fillStyle = glow; ctx.fillRect(-120 * scale, -120 * scale, 240 * scale, 240 * scale);
        for (const [r, angle, brightness] of particles) {
          const radius = Math.sqrt(r) * 125 * scale;
          const arm = Math.floor(angle * 3) * Math.PI * 2 / 3;
          const theta = arm + radius * .035 / scale + rotation * (galaxy ? -1 : 1) + (angle * 3 % 1 - .5) * .8;
          const x = Math.cos(theta) * radius, y = Math.sin(theta) * radius * .48;
          dot(x, y, (.35 + brightness * .75) * scale, galaxy ? `rgba(233,189,137,${.25 + brightness * .7})` : `rgba(174,198,238,${.25 + brightness * .7})`);
        }
        dot(0, 0, 2.2 * scale, "#fff0d4");
        ctx.restore();
        ctx.strokeStyle = "#92a4b744"; ctx.lineWidth = .6; ctx.beginPath(); ctx.arc(cx, cy, 14 * scale, 0, Math.PI * 2); ctx.stroke();
      });
      ctx.setLineDash([3, 5]); line(centers, "#99a9ba55", .6); ctx.setLineDash([]);
      ctx.font = "8px monospace"; ctx.fillStyle = "#9bb3d9"; ctx.textAlign = "center";
      ctx.fillText("GALAXIA A", centers[0][0], height * .85); ctx.fillStyle = "#d4ae81"; ctx.fillText("GALAXIA B", centers[1][0], height * .85);
    }
    function raytracing() {
      const ground = height * .77;
      for (let i = 0; i < 13; i++) {
        const x = width * .5 + (i - 6) * width * .15;
        line([[width * .51, height * .35], [x, height * .91]], "#41505a66", .6);
      }
      for (let i = 0; i < 6; i++) { const y = ground - 32 + i * i * 4; line([[width * .03, y], [width * .97, y]], "#45525c55", .6); }
      const spheres = [[width * .4, ground - height * .15, height * .15, "#a1b7c4", "#243846"], [width * .7, ground - height * .21, height * .21, "#d2b17d", "#645440"], [width * .84, ground - height * .09, height * .09, "#afc2b6", "#43574e"]];
      spheres.forEach(([x, y, r, light, shadow]) => {
        ctx.save(); ctx.translate(x, ground + 2); ctx.scale(1, .16); const g = ctx.createRadialGradient(0, 0, 2, 0, 0, r * 1.3); g.addColorStop(0, "#03090caa"); g.addColorStop(1, "#03090c00"); ctx.fillStyle = g; ctx.beginPath(); ctx.arc(0, 0, r * 1.3, 0, Math.PI * 2); ctx.fill(); ctx.restore();
        const gradient = ctx.createRadialGradient(x - r * .35, y - r * .45, r * .05, x + r * .2, y + r * .3, r * 1.25);
        gradient.addColorStop(0, light); gradient.addColorStop(.5, shadow); gradient.addColorStop(1, "#101c25");
        ctx.fillStyle = gradient; ctx.beginPath(); ctx.arc(x, y, r, 0, Math.PI * 2); ctx.fill();
        ctx.strokeStyle = "#9ea99455"; ctx.lineWidth = .6; ctx.stroke();
      });
      const camera = [width * .11, height * .33], hit = [width * .58, height * .5], bounce = [width * .75, height * .13];
      line([camera, hit, bounce], "#e8ba6fa0", 1);
      ctx.setLineDash([3, 4]); line([[hit[0] - 30, hit[1] + 35], [hit[0] + 35, hit[1] - 45]], "#9cacb677", .8); ctx.setLineDash([]);
      const progress = (time * .3) % 2;
      const a = progress < 1 ? camera : hit, b = progress < 1 ? hit : bounce, p = progress % 1;
      dot(a[0] + (b[0] - a[0]) * p, a[1] + (b[1] - a[1]) * p, 3, "#ffe3a8");
      dot(hit[0], hit[1], 3, "#ffdea4");
      ctx.strokeStyle = "#b7c6c6"; ctx.strokeRect(camera[0] - 14, camera[1] - 9, 17, 18);
      line([[camera[0] + 3, camera[1] - 5], [camera[0] + 10, camera[1] - 10], [camera[0] + 10, camera[1] + 10], [camera[0] + 3, camera[1] + 5]], "#b7c6c6");
      ctx.fillStyle = "#b9aa87"; ctx.font = "8px monospace"; ctx.textAlign = "left"; ctx.fillText("RAYO PRIMARIO", width * .13, height * .25);
    }
    function sentinel() {
      const terrainTop = height * .51, terrainBottom = height * .92;
      ctx.save(); ctx.beginPath(); ctx.rect(width * .06, terrainTop - 15, width * .9, terrainBottom - terrainTop + 15); ctx.clip();
      for (let y = terrainTop; y < terrainBottom; y += 10) {
        const points = [];
        for (let x = width * .06; x <= width * .96; x += 5) points.push([x, y + terrain(x, y) * .7]);
        line(points, `rgba(108,169,139,${.25 + (y - terrainTop) / height * .8})`, .7);
      }
      for (let x = width * .07; x < width * .96; x += 30) {
        const points = [];
        for (let y = terrainTop; y <= terrainBottom; y += 5) points.push([x + Math.sin(y * .025) * 4, y + terrain(x, y) * .7]);
        line(points, "#6b9b7d33", .5);
      }
      const scan = width * (.13 + ((time * .065) % .72));
      const band = ctx.createLinearGradient(scan - 30, 0, scan + 30, 0);
      band.addColorStop(0, "#95d2a500"); band.addColorStop(.5, "#95d2a52b"); band.addColorStop(1, "#95d2a500");
      ctx.fillStyle = band; ctx.fillRect(scan - 30, terrainTop - 15, 60, terrainBottom - terrainTop + 15);
      line([[scan, terrainTop], [scan, terrainBottom]], "#b6ddab88", 1);
      ctx.restore();
      const sx = width * .46 + Math.sin(time * .16) * 7, sy = height * .27;
      ctx.fillStyle = "#93bc8d0b"; ctx.beginPath(); ctx.moveTo(sx, sy + 10); ctx.lineTo(width * .29, height * .71); ctx.lineTo(width * .83, height * .73); ctx.closePath(); ctx.fill();
      ctx.setLineDash([4, 6]); line([[width * .29, height * .71], [sx, sy + 10], [width * .83, height * .73]], "#8dbe9a55", .7); ctx.setLineDash([]);
      ctx.save(); ctx.translate(sx, sy); ctx.rotate(-.34);
      ctx.fillStyle = "#b7c8bc"; ctx.fillRect(-11, -12, 22, 24); ctx.strokeStyle = "#eff0d0"; ctx.strokeRect(-11, -12, 22, 24);
      [-1, 1].forEach((side) => {
        const px = side < 0 ? -68 : 19;
        ctx.fillStyle = "#304850"; ctx.fillRect(px, -17, 49, 34); ctx.strokeStyle = "#7eaaa5"; ctx.lineWidth = .8; ctx.strokeRect(px, -17, 49, 34);
        for (let i = 1; i < 5; i++) line([[px + i * 9.8, -17], [px + i * 9.8, 17]], "#83a7a17a", .6);
        line([[px, 0], [px + 49, 0]], "#83a7a17a", .6);
        line([[side * 11, 0], [side * 19, 0]], "#b7c8bc", 1.5);
      });
      line([[0, 12], [5, 24], [18, 24]], "#d6dabb", 1);
      ctx.beginPath(); ctx.arc(18, 19, 9, 0, Math.PI); ctx.strokeStyle = "#d6dabb"; ctx.stroke();
      ctx.restore();
      ctx.fillStyle = "#a9bba7"; ctx.font = "8px monospace"; ctx.textAlign = "left"; ctx.fillText("A · B* / DIFERENCIA DE FASE", width * .11, height * .43);
    }
    function draw() {
      if (!ctx) return;
      background();
      ({dl: network, nbody: galaxies, ray: raytracing, sentinel})[currentKey]();
    }
    function allowed() { return !manuallyPaused && !motionPreference.matches && !document.hidden && visible; }
    function animate(timestamp) {
      frame = 0;
      if (!allowed()) {lastTimestamp = 0; return;}
      if (lastTimestamp) time += Math.min((timestamp - lastTimestamp) / 1000, .05);
      lastTimestamp = timestamp;
      draw();
      frame = requestAnimationFrame(animate);
    }
    function sync() {
      if (frame) cancelAnimationFrame(frame);
      frame = 0; lastTimestamp = 0;
      if (allowed()) frame = requestAnimationFrame(animate);
      else draw();
    }
    function resize() {
      const rect = canvas.getBoundingClientRect();
      width = rect.width; height = rect.height;
      const ratio = Math.min(window.devicePixelRatio || 1, 2);
      canvas.width = Math.round(width * ratio); canvas.height = Math.round(height * ratio);
      if (ctx) ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
      draw();
    }
    new ResizeObserver(resize).observe(canvas);
    new IntersectionObserver(([entry]) => { visible = entry.isIntersecting; sync(); }, {threshold: .05}).observe(canvas);
    document.addEventListener("visibilitychange", sync);
    return {draw, sync};
  })();

  // Fondo de toda la vista: continúa visible al desplazarse por las métricas.
  // Las cuatro composiciones comparten un reloj y el control de movimiento.
  const atmosphere = (() => {
    const canvas = $("ambient-scene");
    const ctx = canvas.getContext("2d");
    let width = 1, height = 1, time = 0, frame = 0, lastTimestamp = 0;
    const colors = {dl: "174,94,60", nbody: "82,108,165", ray: "157,119,50", sentinel: "65,121,96"};
    let seed = 1209;
    const random = () => {seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 4294967296;};
    const points = Array.from({length: 90}, () => [random(), random(), random()]);
    const stars = Array.from({length: 340}, () => [random(), random(), random()]);
    function line(path, alpha = .2, strokeWidth = 1) {
      ctx.strokeStyle = `rgba(${colors[currentKey]},${alpha})`;
      ctx.lineWidth = strokeWidth;
      ctx.beginPath();
      path.forEach(([x, y], index) => index ? ctx.lineTo(x, y) : ctx.moveTo(x, y));
      ctx.stroke();
    }
    function dot(x, y, radius, alpha = .35, color = colors[currentKey]) {
      ctx.fillStyle = `rgba(${color},${alpha})`;
      ctx.beginPath(); ctx.arc(x, y, radius, 0, Math.PI * 2); ctx.fill();
    }
    function neuralField() {
      const spaceX = Math.max(120, width / 9), spaceY = Math.max(110, height / 7);
      for (let col = -1; col < width / spaceX + 1; col++) {
        for (let row = -1; row < height / spaceY + 1; row++) {
          const sway = Math.sin(time * .12 + row * .8 + col) * 11;
          const a = [col * spaceX + sway, row * spaceY + (col % 2) * spaceY * .4];
          const b = [(col + 1) * spaceX + Math.sin(time * .12 + row * .8 + col + 1) * 11, row * spaceY + ((col + 1) % 2) * spaceY * .4];
          line([a, b], .15);
          line([a, [b[0], b[1] + spaceY]], .09);
          const progress = (time * .16 + (row + col + 20) * .17) % 1;
          if ((row + col) % 3 === 0) dot(a[0] + (b[0] - a[0]) * progress, a[1] + (b[1] - a[1]) * progress, 2.2, .5);
          dot(a[0], a[1], 2.5, .3);
          ctx.strokeStyle = `rgba(${colors.dl},.18)`;
          ctx.beginPath(); ctx.arc(a[0], a[1], 7, 0, Math.PI * 2); ctx.stroke();
        }
      }
    }
    function galaxyField() {
      const radius = Math.min(width * .43, height * .64);
      const centers = [[width * .04, height * .34], [width * .96, height * .73]];
      centers.forEach(([cx, cy], galaxy) => {
        const rgb = galaxy ? "146,114,156" : colors.nbody;
        const haze = ctx.createRadialGradient(cx, cy, 0, cx, cy, radius);
        haze.addColorStop(0, `rgba(${rgb},.09)`); haze.addColorStop(1, `rgba(${rgb},0)`);
        ctx.fillStyle = haze; ctx.fillRect(0, 0, width, height);
        for (const [r, angle, size] of stars) {
          const distance = Math.sqrt(r) * radius;
          const theta = Math.floor(angle * 3) * Math.PI * 2 / 3 + distance / radius * 4 + (angle * 3 % 1) * .6 + time * .025 * (galaxy ? -1 : 1);
          dot(cx + Math.cos(theta) * distance, cy + Math.sin(theta) * distance * .66, .6 + size * 1.4, .16 + size * .2, rgb);
        }
        for (let orbit = 1; orbit <= 3; orbit++) {
          ctx.strokeStyle = `rgba(${rgb},.1)`; ctx.lineWidth = .8;
          ctx.beginPath(); ctx.ellipse(cx, cy, radius * orbit / 3, radius * orbit / 3 * .66, 0, 0, Math.PI * 2); ctx.stroke();
        }
      });
      points.slice(0, 35).forEach(([x, y, r]) => dot(x * width, y * height, .6 + r, .2));
    }
    function rayField() {
      const spacing = Math.max(125, width / 9);
      const reflectionY = height * .62;
      for (let ray = -3; ray < width / spacing + 3; ray++) {
        const start = [ray * spacing + Math.sin(time * .07) * 22, -30];
        const hit = [start[0] + height * .23, reflectionY];
        const end = [hit[0] + height * .27, -30];
        line([start, hit, end], ray % 3 === 0 ? .21 : .11);
        const progress = (time * .09 + (ray + 10) * .11) % 2;
        const a = progress < 1 ? start : hit, b = progress < 1 ? hit : end;
        dot(a[0] + (b[0] - a[0]) * (progress % 1), a[1] + (b[1] - a[1]) * (progress % 1), 2, .5);
        dot(hit[0], hit[1], 3, .2);
      }
      line([[0, reflectionY], [width, reflectionY]], .11);
      for (let i = 0; i < 3; i++) {
        ctx.strokeStyle = `rgba(${colors.ray},.12)`;
        ctx.beginPath(); ctx.arc(width * (i % 2 ? .95 : .05), height * (.25 + i * .3), 55 + i * 34, 0, Math.PI * 2); ctx.stroke();
      }
    }
    function terrainField() {
      for (let y = -40; y < height + 50; y += 27) {
        const path = [];
        for (let x = -20; x < width + 20; x += 14) {
          const wave = Math.sin(x * .008 + y * .013) * 25 + Math.cos(x * .014 - y * .005) * 19;
          path.push([x, y + wave + Math.sin(time * .08 + x * .003) * 3]);
        }
        line(path, .12);
      }
      const scanX = ((time * 26 + width * .2) % (width + 180)) - 90;
      const band = ctx.createLinearGradient(scanX - 65, 0, scanX + 65, 0);
      band.addColorStop(0, "rgba(65,121,96,0)"); band.addColorStop(.5, "rgba(65,121,96,.07)"); band.addColorStop(1, "rgba(65,121,96,0)");
      ctx.fillStyle = band; ctx.fillRect(scanX - 65, 0, 130, height);
      line([[scanX, 0], [scanX, height]], .24);
      points.slice(0, 12).forEach(([x, y]) => {
        const cx = x * width, cy = y * height;
        line([[cx - 5, cy], [cx + 5, cy]], .22); line([[cx, cy - 5], [cx, cy + 5]], .22);
      });
    }
    function draw() {
      if (!ctx) return;
      ctx.clearRect(0, 0, width, height);
      ({dl: neuralField, nbody: galaxyField, ray: rayField, sentinel: terrainField})[currentKey]();
    }
    function allowed() {return !manuallyPaused && !motionPreference.matches && !document.hidden;}
    function animate(timestamp) {
      frame = 0;
      if (!allowed()) {lastTimestamp = 0; return;}
      // Limitar el fondo a 24 fps; el contenido y el scroll mantienen su fluidez.
      if (!lastTimestamp || timestamp - lastTimestamp >= 1000 / 24) {
        if (lastTimestamp) time += Math.min((timestamp - lastTimestamp) / 1000, .1);
        lastTimestamp = timestamp;
        draw();
      }
      frame = requestAnimationFrame(animate);
    }
    function sync() {
      if (frame) cancelAnimationFrame(frame);
      frame = 0; lastTimestamp = 0;
      draw();
      if (ctx && allowed()) frame = requestAnimationFrame(animate);
    }
    function resize() {
      width = window.innerWidth; height = window.innerHeight;
      const ratio = Math.min(window.devicePixelRatio || 1, 1.5);
      canvas.width = Math.round(width * ratio); canvas.height = Math.round(height * ratio);
      if (ctx) ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
      draw();
    }
    window.addEventListener("resize", resize);
    document.addEventListener("visibilitychange", sync);
    resize();
    return {draw, sync};
  })();

  function updateMotionButton() {
    const paused = manuallyPaused || motionPreference.matches;
    const button = $("motion-toggle");
    button.setAttribute("aria-pressed", String(paused));
    button.setAttribute("aria-label", paused ? "Activar animaciones de cabecera y fondo" : "Pausar animaciones de cabecera y fondo");
    button.querySelector("span").textContent = paused ? "Activar" : "Pausar";
    button.querySelector("svg").innerHTML = paused ? '<path d="m5 3 7 5-7 5Z"/>' : '<path d="M5 3v10M11 3v10"/>';
    if (motionPreference.matches) {
      button.disabled = true;
      button.title = "Animación detenida por tu preferencia de movimiento reducido";
      button.querySelector("span").textContent = "Sin movimiento";
    } else {button.disabled = false; button.title = "";}
    scene.sync();
    atmosphere.sync();
  }
  $("equipment-toggle").addEventListener("click", () => {
    sentinelEquipmentB = !sentinelEquipmentB;
    selectProblem("sentinel");
  });
  $("motion-toggle").addEventListener("click", () => {manuallyPaused = !manuallyPaused; updateMotionButton();});
  motionPreference.addEventListener("change", updateMotionButton);
  window.addEventListener("hashchange", () => {
    const key = location.hash.slice(1);
    if (problems[key]) {
      selectProblem(key);
      $("problem-content").scrollIntoView({block: "start", behavior: motionPreference.matches ? "instant" : "smooth"});
    }
  });
  if (!window.METRICAS) {
    $("reference-note").textContent = "No se pudieron cargar los datos. Ejecuta python3 generar_datos.py desde la carpeta defensa.";
    return;
  }
  selectProblem(location.hash.slice(1));
  updateMotionButton();
})();
