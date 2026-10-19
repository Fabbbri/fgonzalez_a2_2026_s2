/* Ilustraciones conceptuales deterministas. No ejecutan los benchmarks. */
(() => {
  'use strict';
  const $ = id => document.getElementById(id);
  const canvas = $('film'), c = canvas.getContext('2d');
  const film = window.FILM, duration = film.duration;
  const colors = ['#73a8ff', '#bd91ff', '#ffbf72', '#5ce5be'];
  let subtitles = true, t = Number(new URLSearchParams(location.search).get('t')) || 0, playing = !matchMedia('(prefers-reduced-motion: reduce)').matches, last = null, recording = false;
  if (new URLSearchParams(location.search).has('t')) playing = false;
  t = Math.max(0, Math.min(duration, t));
  const clamp = x => Math.max(0, Math.min(1, x));
  function text(s,x,y,size=28,color='#eaf2ff',align='left') {c.fillStyle=color;c.font=`${size >= 50 ? 650 : 400} ${size}px system-ui, sans-serif`;c.textAlign=align;c.fillText(s,x,y);}
  function line(x,y,xx,yy,color,width=2) {c.beginPath();c.moveTo(x,y);c.lineTo(xx,yy);c.strokeStyle=color;c.lineWidth=width;c.stroke();}
  function dot(x,y,r,color) {c.beginPath();c.arc(x,y,r,0,Math.PI*2);c.fillStyle=color;c.fill();}
  function box(x,y,w,h,label,color) {c.fillStyle='#11233a';c.fillRect(x,y,w,h);c.strokeStyle=color;c.lineWidth=2;c.strokeRect(x,y,w,h);if(label)text(label,x+w/2,y+h/2+10,28,color,'center');}
  function arrow(x,y,xx,yy,color) {line(x,y,xx,yy,color,4);const a=Math.atan2(yy-y,xx-x);line(xx,yy,xx-17*Math.cos(a-.5),yy-17*Math.sin(a-.5),color,4);line(xx,yy,xx-17*Math.cos(a+.5),yy-17*Math.sin(a+.5),color,4);}
  function caption(a,b) {text(a,100,815,32);text(b,100,859,23,'#aabbd1');}
  function dl(s) {
    // The displayed units stand for groups of neurons, not the full layer sizes.
    const counts=[9,8,6,4], xs=[490,800,1110,1420];
    const ys=counts.map(n=>Array.from({length:n},(_,j)=>330+j*400/(n-1)));
    for(let k=0;k<3;k++)for(const y of ys[k])for(const yy of ys[k+1]) {
      line(xs[k],y,xs[k+1],yy,'#233d60',1.5);
      const phase=(s*.42-k*.32)%1;
      if(phase>=0)dot(xs[k]+310*phase,y+(yy-y)*phase,3.5,colors[0]);
    }
    xs.forEach((x,k)=>{ys[k].forEach((y,j)=>dot(x,y,14+3*Math.sin(s*3-k+j),colors[0]));text(['Entrada','Capa oculta','Capa oculta','Salida'][k],x,770,25,'#b4c9e8','center');});
    box(120,430,220,220,'',colors[0]);text('7',230,590,140,'#fff','center');arrow(355,540,440,540,colors[0]);
    text('Píxeles → capas → salida',1510,420,24,colors[0]);text('Neuronas',1540,465,25,'#aabbd1');
    text('Predicción',1540,575,25,'#aabbd1');text('7',1585,660,78,colors[0]);
    caption('Propagar señales → calcular el error → actualizar los pesos', 'El entrenamiento contiene operaciones sobre matrices que pueden repartirse entre trabajadores.');
  }
  function nbody(s) {
    const points=[];
    for(let g=0;g<2;g++)for(let i=0;i<220;i++) {
      const r=25+Math.sqrt((i+.5)/220)*210,a=i*2.399963+s*(g?-.18:.18)+r*.009;
      const cx=460+g*480+(g?-1:1)*70*Math.sin(s*.13), cy=530+(g?35:-35);
      const p=[cx+Math.cos(a)*r,cy+Math.sin(a)*r*.68];points.push(p);dot(...p,1.8+(i%4)*.4,g?'#6bd8ff':colors[1]);
    }
    const selected=points[50];
    for(let j=0;j<points.length;j+=17)line(...selected,...points[j],'#bd91ff30',1);
    dot(...selected,8,'#fff');
    box(1360,345,420,100,'Fuerzas de cada cuerpo',colors[1]);
    arrow(1570,460,1570,510,colors[1]);box(1360,525,420,100,'Actualizar posiciones',colors[1]);
    text('Interacciones O(N²)',1360,700,30,colors[1]);
    caption('Cada cuerpo recibe la atracción gravitacional de los demás', 'Las fuerzas se calculan por bloques; las fases se sincronizan antes del siguiente paso.');
  }
  function ray(s) {
    // Diagram of a camera ray, an intersection and a reflected direction.
    box(120,460,185,130,'Cámara',colors[2]);
    for(let i=0;i<11;i++)line(330+i*66,730,900+i*60,300,'#21334a');
    for(let i=0;i<9;i++)line(330,330+i*50,1200,330+i*50,'#21334a');
    const gradient=c.createRadialGradient(875,440,10,940,520,135);gradient.addColorStop(0,'#ffe5b8');gradient.addColorStop(.5,'#d88a42');gradient.addColorStop(1,'#442b33');
    dot(940,520,135,gradient);dot(660,630,70,'#5389b8');
    arrow(315,520,815,520,colors[2]);arrow(815,520,540,310,colors[2]);
    const u=(s*.25)%1, path=u<.65?[315+(815-315)*u/.65,520]:[815+(540-815)*(u-.65)/.35,520+(310-520)*(u-.65)/.35];
    dot(...path,10,'#fff');text('Intersección',825,705,25,colors[2]);text('Rebote',450,285,25,colors[2]);
    const done=Math.min(80,Math.floor((s/10)*80));
    text('Imagen dividida en tiles',1350,315,28,colors[2]);
    for(let row=0;row<8;row++)for(let col=0;col<10;col++) {
      const i=row*10+col,x=1350+col*42,y=360+row*42;
      c.fillStyle=i<done?`hsl(${30+col*3} 65% ${30+row*4}%)`:'#14243b';c.fillRect(x,y,39,39);
      if(i===done){c.strokeStyle='#fff';c.lineWidth=3;c.strokeRect(x,y,39,39);}
    }
    caption('Cada región de la imagen puede calcularse de forma independiente', 'Los trabajadores toman tiles; cada píxel conserva las mismas muestras y reglas de trazado.');
  }
  // Shared motion primitives use the timeline rather than random state so that
  // seeking reproduces exactly the same frame, including particle positions.
  const ease = v => {v=clamp(v);return v*v*(3-2*v);};
  const fract = x => x-Math.floor(x);
  function panel(x,y,w,h,color='#263b57') {c.fillStyle='#0b1a2c';c.beginPath();c.roundRect(x,y,w,h,20);c.fill();c.strokeStyle=color;c.lineWidth=2;c.stroke();}
  function glow(x,y,r,color) {const g=c.createRadialGradient(x,y,0,x,y,r);g.addColorStop(0,color);g.addColorStop(1,'transparent');dot(x,y,r,g);}
  function wrap(s,x,y,width,size=28,color='#eaf2ff',gap=40) {
    c.font=`400 ${size}px system-ui, sans-serif`;let row='';
    for(const word of s.split(' ')){const next=row?`${row} ${word}`:word;if(c.measureText(next).width>width&&row){text(row,x,y,size,color);y+=gap;row=word;}else row=next;}if(row)text(row,x,y,size,color);return y;
  }
  function queue(s,parallel=false) {
    const lanes=parallel?4:1,cycle=fract(s/7),done=Math.floor(cycle*24);
    text('MISMA CANTIDAD DE TRABAJO',960,318,22,'#9db2ce','center');
    for(let k=0;k<24;k++){
      const lane=k%lanes,order=Math.floor(k/lanes),x=parallel?390+order*185:180+(k%12)*130,y=parallel?390+lane*90:455+Math.floor(k/12)*130;
      const complete=k<done;panel(x,y,100,64,complete?'#5ce5be':'#365277');text(String(k+1).padStart(2,'0'),x+50,y+42,25,complete?'#5ce5be':'#6582a6','center');
    }
    caption(parallel?'Varios trabajadores pueden avanzar a la vez':'Una tarea después de otra', 'Esquema didáctico: los tiempos animados no representan mediciones.');
  }
  function intro(s) {
    const icons=['7','✦','↗','≋'],names=['Deep Learning','N-body','Ray Tracing','Sentinel-1'];
    for(let k=0;k<4;k++){
      const x=110+k*435,y=350-18*Math.sin(s*.7+k),alpha=ease((s-k*.45)/1.5);
      c.save();c.globalAlpha=alpha;panel(x,y,395,340,colors[k]);glow(x+198,y+140,155,colors[k]+'25');text(icons[k],x+198,y+190,130,colors[k],'center');text(names[k],x+198,y+285,32,'#fff','center');c.restore();
    }
    const a=ease((s-3)/2);c.save();c.globalAlpha=a;text('¿Y si más hilos no fueran siempre mejores?',960,805,43,'#fff','center');c.restore();
  }
  function scheduling(s,fine) {
    const palette=['#73a8ff','#bd91ff','#ffbf72','#5ce5be'];
    text(fine?'UN HILO REAL · CUATRO TAREAS LÓGICAS':'HILOS REALES · AVANCE Y ESPERAS',960,315,24,'#aabbd1','center');
    for(let row=0;row<4;row++){
      const y=380+row*85;text(`Tarea ${row+1}`,125,y+40,28,palette[row]);
      for(let col=0;col<20;col++){
        const x=350+col*68,active=fine?col%4===row:((col+row*2)%9<6),shown=col<Math.floor(fract(s/8)*21);
        c.fillStyle=shown?(active?palette[row]:'#29384a'):'#142238';c.fillRect(x,y,58,54);
        if(shown&&!active&&!fine)text('Ⅱ',x+29,y+37,24,'#aabbd1','center');
      }
    }
    const x=350+fract(s/8)*1360;line(x,365,x,730,'#fff',3);
    caption(fine?'Turnos round-robin, independientemente de bloqueos':'Un bloqueo suspende al hilo; el SO planifica la ejecución',fine?'Tareas intercaladas ≠ varios núcleos calculando simultáneamente.':'Las pausas dibujadas explican el concepto, no la duración real de las esperas.');
  }
  function hardware(s) {
    const isSMT=s>=5; text(isSMT?'SMT: DOS CONTEXTOS COMPARTEN CADA NÚCLEO':'CMP: TRABAJO EN NÚCLEOS FÍSICOS DISTINTOS',960,320,26,'#aabbd1','center');
    for(let k=0;k<4;k++){
      const x=140+k*440;panel(x,385,330,340,isSMT?'#5ce5be':'#73a8ff');text(`NÚCLEO ${k+1}`,x+165,435,24,'#fff','center');
      box(x+30,470,isSMT?125:270,70,'Hilo A','#73a8ff');if(isSMT)box(x+175,470,125,70,'Hilo B','#bd91ff');
      for(let j=0;j<6;j++){const xx=x+35+j*44;c.fillStyle=(j+Math.floor(s*4))%3===0?'#5ce5be':'#2d4963';c.fillRect(xx,590,30,50);}
      text('Recursos de ejecución',x+165,685,22,'#aabbd1','center');
    }
    caption(isSMT?'Compartir un núcleo no crea otro núcleo físico':'CMP fija trabajadores en núcleos físicos distintos','SMT on/off desde Linux · topología real: 8 núcleos / 12 CPU lógicas con SMT.');
  }
  function learn(s) {
    const steps=['Predicción','Error','Actualizar pesos'],active=Math.floor(s/3.34);
    for(let k=0;k<3;k++){const x=150+k*580;panel(x,370,460,270,k===active?colors[0]:'#2b405c');text(['7 ?','ŷ − y','W ← W − η∇L'][k],x+230,490,k===2?36:66,colors[0],'center');text(steps[k],x+230,590,30,'#fff','center');if(k<2)arrow(x+475,500,x+560,500,'#6b8bb4');}
    c.beginPath();c.moveTo(310,740);c.bezierCurveTo(1300,870,1600,870,1640,675);c.strokeStyle='#73a8ff';c.lineWidth=3;c.stroke();text('Repetir el entrenamiento',960,745,27,'#aabbd1','center');
  }
  function matrix(s) {
    text('ACTUALIZACIONES REPARTIDAS',650,330,25,colors[0],'center');
    for(let y=0;y<9;y++)for(let x=0;x<14;x++){const px=160+x*66,py=375+y*38,band=Math.floor(y/3),on=fract(s*.35-band*.2)>.2;c.fillStyle=on?colors[band]:'#19304d';c.fillRect(px,py,58,30);}
    ['Trabajador 1','Trabajador 2','Trabajador 3'].forEach((label,k)=>{arrow(1100,425+k*115,1210,425+k*115,colors[k]);box(1240,390+k*115,480,75,label,colors[k]);});
    caption('Solo una parte del entrenamiento se distribuye','Forward y backpropagation secuenciales · actualizaciones paralelas en esta implementación.');
  }
  function gravity(s) {
    const n=s<5?12:24,cx=650,cy=535,r=220;
    for(let j=0;j<n;j++){const a=j/n*Math.PI*2,p=[cx+Math.cos(a)*r,cy+Math.sin(a)*r];line(cx,cy,...p,'#bd91ff55',2);dot(...p,8,colors[1]);const u=fract(s*.5+j/n);dot(cx+Math.cos(a)*r*u,cy+Math.sin(a)*r*u,4,'#fff');}
    glow(cx,cy,80,'#bd91ff44');dot(cx,cy,17,'#fff');
    text(s<5?'N':'2N',1330,470,100,colors[1],'center');text(s<5?'≈ N² interacciones':'≈ 4N² interacciones',1330,570,40,'#fff','center');
    caption('El costo del método directo crece aproximadamente como N²','Diagrama de interacciones; no es una integración física de las órbitas.');
  }
  function barrier(s) {
    const cycle=fract(s/7),progress=clamp(cycle*1.5),ends=[.48,.60,.73,.85];
    for(let j=0;j<4;j++){
      const y=370+j*85; text(`Bloque ${j+1}`,130,y+40,27,colors[1]);
      c.fillStyle='#172941';c.fillRect(380,y,950,54);c.fillStyle=colors[1];c.fillRect(380,y,950*clamp(progress/ends[j]),54);
      text(progress>=ends[j]?(progress>=ends[3]?'Listo para avanzar':'Esperando'):'Calculando',1430,y+40,25,progress>=ends[3]?'#5ce5be':'#aabbd1');
    }
    line(1330,350,1330,720,'#fff',3);text('Barrera',1330,320,26,'#fff','center');
    caption('Todos completan la fase antes de continuar','Representación conceptual: las barras no están escaladas a tiempos de los benchmarks.');
  }
  function samples(s) {
    panel(180,365,310,310,colors[2]);text('Un píxel',335,725,28,colors[2],'center');
    for(let k=0;k<8;k++){const a=k*2.39996,x=335+Math.cos(a)*90,y=520+Math.sin(a)*90;dot(x,y,8,colors[2]);const u=fract(s*.35+k/8);line(x,y,1050+Math.cos(a)*200,440+Math.sin(a)*160,'#ffbf7230',2);dot(x+(1050+Math.cos(a)*200-x)*u,y+(440+Math.sin(a)*160-y)*u,5,'#fff');}
    const g=c.createRadialGradient(1160,430,0,1210,500,150);g.addColorStop(0,'#fff2ce');g.addColorStop(1,'#805739');dot(1210,500,150,g);
    arrow(1370,500,1640,350,colors[2]);arrow(1640,350,1550,655,colors[2]);
    text('8 muestras por píxel',1100,750,31,colors[2]);text('Hasta 12 rebotes',1460,750,26,'#aabbd1');
  }
  function tileWork(s,radar=false) {
    const palette=radar?['#5ce5be','#73a8ff','#bd91ff','#ffbf72']:['#ffbf72','#73a8ff','#bd91ff','#5ce5be'];
    const done=Math.min(96,Math.floor(ease(s/9)*96));
    for(let j=0;j<96;j++){
      const x=170+(j%12)*71,y=355+Math.floor(j/12)*49;
      c.fillStyle=j<done?(radar?`hsl(${(j%12)*18+Math.floor(j/12)*24} 65% 45%)`:palette[j%4]):'#16283f';c.fillRect(x,y,66,44);
      if(j>=done&&j<done+4){c.strokeStyle=palette[j%4];c.lineWidth=3;c.strokeRect(x,y,66,44);}
    }
    for(let k=0;k<4;k++){const y=365+k*100;box(1310,y,430,70,`Trabajador ${k+1}`,palette[k]);arrow(1060,440+k*60,1280,y+35,palette[k]);}
    caption(radar?'Un producto complejo por píxel':'El siguiente tile disponible va al trabajador libre',radar?'El costo por píxel y el acceso a memoria condicionan el beneficio de repartirlo.':'Las regiones de costo desigual se distribuyen mediante una cola compartida.');
  }
  function radar(s) {
    c.save();c.translate(950,560);c.scale(1,.48);c.rotate(-.22);
    for(let j=0;j<16;j++){
      c.beginPath();for(let x=-650;x<=650;x+=12){const y=j*38-300+Math.sin(x*.008+j*.32)*70+Math.cos(x*.014-j*.4)*28;if(x===-650)c.moveTo(x,y);else c.lineTo(x,y);}c.strokeStyle=`rgba(92,229,190,${.15+j*.025})`;c.lineWidth=2;c.stroke();
    }c.restore();
    const x=370+fract(s/10)*1150,y=340;glow(x,y,70,'#73a8ff55');box(x-35,y-28,70,55,'','#b8d6ff');box(x-135,y-15,85,28,'','#73a8ff');box(x+50,y-15,85,28,'','#73a8ff');
    c.beginPath();c.moveTo(x,y+35);c.lineTo(x-200,705);c.lineTo(x+290,705);c.closePath();c.fillStyle='#5ce5be14';c.fill();line(x,y+35,x+40,680,'#5ce5be',3);
    text('Pasada A',230,350,30,colors[3]);text('Pasada B',1520,350,30,colors[0]);
    caption('Radar de apertura sintética · datos complejos','Terreno y barrido ilustrativos; las adquisiciones del experimento se conservan en los datos del proyecto.');
  }
  function phase(s) {
    const a=.7+s*.15,b=-.5+s*.15;
    for(let k=0;k<2;k++){
      const cx=420+k*1000,cy=520;panel(cx-250,310,500,425,k?colors[0]:colors[3]);
      c.beginPath();c.arc(cx,cy,140,0,Math.PI*2);c.strokeStyle='#34526c';c.lineWidth=2;c.stroke();line(cx-170,cy,cx+170,cy,'#34526c');line(cx,cy-170,cx,cy+170,'#34526c');
      const angle=k?b:a;arrow(cx,cy,cx+Math.cos(angle)*130,cy-Math.sin(angle)*130,k?colors[0]:colors[3]);text(k?'Fase B':'Fase A',cx,355,30,'#fff','center');text(k?'φB':'φA',cx,700,32,'#fff','center');
    }
    text('A × B*',920,450,53,colors[3],'center');text('Δφ = φA − φB',920,560,39,'#fff','center');
    caption('El producto conserva la diferencia de fase','Las franjas por sí solas no son una medida directa de desplazamiento.');
  }
  function chart(key,s) {
    const data=film.results[key],color=colors[{dl:0,nbody:1,ray:2,sentinel:3}[key]],base=710,max=key==='ray'?5:4,x0=350,w=1200;
    // The heights and error bars use actual reported means and approximate IC95.
    for(let val=0;val<=max;val++){const y=base-val*85;line(x0,y,x0+w,y,val===1?'#eaf2ff80':'#233b55',val===1?2:1);text(`${val}×`,300,y+8,24,'#9db2ce','right');}
    const growth=ease(s/2);
    data.values.forEach((value,j)=>{
      const x=480+j*300,height=value*85*growth;c.fillStyle=color;c.beginPath();c.roundRect(x-65,base-height,130,height,8);c.fill();
      const y=base-height,ci=data.ci[j]*85*growth;line(x,y-ci,x,y+ci,'#fff',3);line(x-12,y-ci,x+12,y-ci,'#fff',3);line(x-12,y+ci,x+12,y+ci,'#fff',3);
      text(`${value.toFixed(2)}×`,x,y-25,34,'#fff','center');text(`${[1,2,4,8][j]} trabajador${j?'es':''}`,x,760,25,'#afc4df','center');
    });
    text('SPEEDUP CMP / MAYOR ES MEJOR',350,300,23,color);text('1× = secuencial',1630,base-85+8,22,'#fff');
    text(data.n,350,825,24,'#b9cce6');
  }
  function memory(s) {
    // Both configurations are user-reported. Packet flow is conceptual: neither
    // bandwidth nor a causal RAM effect was measured in a controlled comparison.
    const palette=['#ffbf72','#5ce5be'];
    for(let k=0;k<2;k++){
      const x=100+k*900,color=palette[k];
      panel(x,285,820,465,color);
      text(k?'Segundo equipo':'Equipo de pruebas',x+410,335,30,'#eaf2ff','center');
      text(k?'RAM · 8 533 MT/s':'RAM · 3 200 MT/s',x+410,393,46,color,'center');
      const rx=x+160,ry=423;
      // DIMM: printed circuit, memory chips, gold contacts and key notch.
      c.fillStyle='#164d42';c.beginPath();c.roundRect(rx,ry,500,87,8);c.fill();
      c.strokeStyle='#4d9e7d';c.lineWidth=2;c.stroke();
      for(let chip=0;chip<8;chip++){
        c.fillStyle='#0a1622';c.fillRect(rx+18+chip*59,ry+15,46,46);
        line(rx+23+chip*59,ry+66,rx+23+chip*59,ry+79,'#40886c',2);
      }
      for(let pin=0;pin<34;pin++){if(pin===17||pin===18)continue;c.fillStyle='#d5b969';c.fillRect(rx+7+pin*14.4,ry+87,9,12);}
      const pipeY=558;
      line(x+170,pipeY,x+650,pipeY,'#304860',12);
      // A narrower illustrated path and a queue explain the hypothesis, rather
      // than turning the reported 3200/8533 MT/s values into fabricated bandwidth.
      for(let packet=0;packet<7;packet++){
        const u=fract(s*(k?.22:.13)+packet/7);
        c.fillStyle=color;c.fillRect(x+175+u*450,pipeY-5,16,10);
      }
      if(!k)for(let packet=0;packet<4;packet++){
        c.globalAlpha=.45+.3*Math.sin(s*3+packet);c.fillStyle=color;c.fillRect(x+115+packet*18,pipeY-28,12,10);
      }
      c.globalAlpha=1;
      for(let core=0;core<4;core++){
        const cx=x+170+core*130;panel(cx,603,100,58,'#35516a');
        const fed=k?fract(s*.9+core*.17)>.12:fract(s*.4+core*.22)>.60;
        dot(cx+21,632,5,fed?color:'#496077');text('CPU',cx+61,641,20,fed?color:'#7891ac','center');
        line(cx+50,pipeY+14,cx+50,592,'#45617b',2);
      }
      text(k?'Mejora observada por el grupo':'Sin mejora en la campaña actual',x+410,710,27,color,'center');
    }
    if(s<10){
      text('Tasas de transferencia de RAM informadas por el grupo',960,795,24,'#aabbd1','center');
      text('Flujo de datos ilustrativo; no representa ancho de banda medido.',960,841,24,'#aabbd1','center');
    }else{
      text('Si la memoria limita el flujo, más núcleos pueden esperar datos',960,795,31,'#eaf2ff','center');
      text('Hipótesis: también cambian CPU, caché, canales y configuración.',960,841,26,'#aabbd1','center');
    }
  }
  function evidence(s) {
    const cards=[['200','Ray · N-body · Sentinel'],['9–20','DL · campaña preliminar'],['IC95','Incertidumbre de la media']];
    cards.forEach((a,k)=>{const x=130+k*580;panel(x,350,500,255,colors[k]);text(a[0],x+250,470,80,colors[k],'center');text(a[1],x+250,550,24,'#fff','center');});
    text('perf',155,720,55,colors[3]);text('Ciclos  ·  Instrucciones  ·  Caché  ·  Cambios de contexto',410,715,30,'#aabbd1');
    const x=410+fract(s/6)*1310;line(410,770,1720,770,'#2b405c',4);dot(x,770,7,colors[3]);
    caption('El perfilado observa el proceso completo','Stalls no disponibles y cobertura baja se declaran; no se sustituyen por cero.');
  }
  function compare(s) {
    const keys=['dl','nbody','ray','sentinel'],growth=ease(s/2);
    keys.forEach((key,k)=>{
      const data=film.results[key],y=365+k*110,value=data.values[2],ci=data.ci[2];text(data.name,160,y+40,31,colors[k]);
      c.fillStyle=colors[k];c.fillRect(490,y,Math.max(1,value*270*growth),58);text(`${value.toFixed(2)}×${key==='dl'?' *':''}`,510+value*270*growth,y+41,32,'#fff');
      const xx=490+value*270*growth;line(xx-ci*270,y+29,xx+ci*270,y+29,'#fff',3);
    });
    line(760,330,760,775,'#ffffff90',2);text('1×',760,305,26,'#fff','center');
    caption('CMP · cuatro trabajadores · cada problema frente a su propia base','* DL preliminar. Barras: speedup e IC95. Los tiempos absolutos entre problemas no son equivalentes.');
  }
  function ending(s) {
    const words=['Trabajo independiente','Costo de coordinación','Recursos del hardware'];
    words.forEach((word,k)=>{const x=140+k*580;c.save();c.globalAlpha=ease((s-k*.6)/1.5);panel(x,365,500,260,colors[k]);text(['01','02','03'][k],x+40,430,34,colors[k]);wrap(word,x+40,505,420,37,'#fff',51);c.restore();});
    text('La respuesta está en medir.',960,770,57,'#fff','center');
  }
  function thanks(s) {
    // Four motifs recall the film's topics. Motion settles before the last frame,
    // and every position derives from time so seeking/export stay deterministic.
    const reveal=ease(s/1.5),tau=Math.PI*2;
    // Keep the extended farewell alive; ease motion to rest only at the end.
    const stopAt=film.shots.find(shot=>shot.kind==='thanks').duration-.25;
    const slowAt=Math.max(0,stopAt-1),tail=clamp(s-slowAt);
    const motion=Math.min(s,slowAt)+tail-tail*tail/2;
    c.save();
    c.globalCompositeOperation='screen';
    glow(320,250,470,'#397dff24');glow(1590,230,510,'#b880ff26');
    glow(290,850,420,'#ffb66517');glow(1580,860,460,'#47eec923');
    // Fine elliptical traces bind the four visual worlds into one composition.
    for(let ring=0;ring<3;ring++){
      c.save();c.translate(960,530);c.rotate(-.16+ring*.12);
      c.beginPath();c.ellipse(0,0,710+ring*65,305+ring*52,0,0,tau);
      c.strokeStyle=['#73a8ff22','#bd91ff24','#5ce5be20'][ring];c.lineWidth=1.5;c.stroke();
      const angle=motion*.13+ring*2.1;
      const px=Math.cos(angle)*(710+ring*65),py=Math.sin(angle)*(305+ring*52);
      glow(px,py,30,colors[ring]+'45');dot(px,py,4,colors[ring]);c.restore();
    }
    for(let i=0;i<100;i++){
      const x=fract(i*.618034+motion*.0009*(1+i%3))*1920;
      const y=fract(i*.414214-motion*.0012)*1080;
      const shimmer=.18+.22*(.5+.5*Math.sin(motion*.8+i));
      c.globalAlpha=shimmer*reveal;dot(x,y,i%13===0?2.2:1,colors[i%4]);
    }
    c.restore();
    // Deep Learning: a constellation of nodes with pulses across connections.
    c.save();c.globalAlpha*=ease((s-.15)/1.2);
    const layers=[3,4,3],xs=[165,310,455];
    const nodes=layers.map((count,k)=>Array.from({length:count},(_,j)=>[xs[k],175+j*160/(count-1)]));
    for(let k=0;k<2;k++)for(const from of nodes[k])for(const to of nodes[k+1]){
      line(...from,...to,'#73a8ff38',1.2);
      const u=fract(motion*.32+k*.24+from[1]*.003);
      dot(from[0]+(to[0]-from[0])*u,from[1]+(to[1]-from[1])*u,2.5,'#c2d9ff');
    }
    nodes.flat().forEach(([x,y],i)=>{glow(x,y,18,'#73a8ff25');dot(x,y,4+i%2,'#83b5ff');});
    text('DEEP LEARNING',310,385,18,'#80a8d9','center');c.restore();
    // N-body: two luminous spiral arms in an inclined orbital plane.
    c.save();c.globalAlpha*=ease((s-.3)/1.2);c.translate(1580,260);c.rotate(-.24);
    for(let i=0;i<230;i++){
      const r=15+Math.sqrt(i/230)*162,angle=i*2.399963+r*.024+motion*.18;
      const x=Math.cos(angle)*r,y=Math.sin(angle)*r*.48;
      dot(x,y,i%11===0?2.4:1.25,i%3?'#bc9ae8':'#dcd2ff');
    }
    glow(0,0,75,'#bd91ff60');dot(0,0,5,'#ece0ff');c.restore();
    text('N-BODY',1580,385,18,'#ad94cf','center');
    // Ray tracing: a moving light pulse, a sphere and reflected segments.
    c.save();c.globalAlpha*=ease((s-.45)/1.2);
    const points=[[135,872],[335,824],[224,710],[472,742]];
    for(let i=0;i<points.length-1;i++)line(...points[i],...points[i+1],'#ffbf7265',2);
    const g=c.createRadialGradient(351,801,3,380,824,49);g.addColorStop(0,'#fff2c8');g.addColorStop(.45,'#e8ae6d');g.addColorStop(1,'#4d3432');dot(380,824,49,g);
    const travel=fract(motion*.43)*(points.length-1),segment=Math.min(2,Math.floor(travel)),u=travel-segment;
    const from=points[segment],to=points[segment+1],px=from[0]+(to[0]-from[0])*u,py=from[1]+(to[1]-from[1])*u;
    glow(px,py,24,'#ffbf7240');dot(px,py,4,'#fff1cf');
    text('RAY TRACING',310,955,18,'#c1a17d','center');c.restore();
    // Sentinel: concentric waves, a radar sweep and terrain-like contour lines.
    c.save();c.globalAlpha*=ease((s-.6)/1.2);c.translate(1580,826);
    for(let ring=1;ring<=4;ring++){
      c.beginPath();c.ellipse(0,0,ring*37,ring*24,0,0,tau);c.strokeStyle='#5ce5be32';c.lineWidth=1.5;c.stroke();
    }
    const scan=-1.5+motion*.45;
    c.beginPath();c.moveTo(0,0);c.ellipse(0,0,148,96,0,scan-.4,scan);c.closePath();c.fillStyle='#5ce5be16';c.fill();
    line(0,0,Math.cos(scan)*148,Math.sin(scan)*96,'#89edcc',2);
    dot(0,0,4,'#c0ffe9');c.restore();text('SENTINEL-1',1580,955,18,'#83b8aa','center');
    // A dark radial veil protects the title from the luminous background.
    const veil=c.createRadialGradient(960,530,160,960,530,590);
    veil.addColorStop(0,'#081322f5');veil.addColorStop(.65,'#081322b8');veil.addColorStop(1,'#08132200');
    c.fillStyle=veil;c.fillRect(0,0,1920,1080);
    c.save();c.translate(960,520+(1-reveal)*30);c.scale(.965+.035*reveal,.965+.035*reveal);
    c.globalAlpha*=reveal;c.shadowColor='#b8dfff40';c.shadowBlur=22;
    text('Gracias por ver',0,0,108,'#f3f7ff','center');c.shadowBlur=0;
    text('Cuatro mundos. Una pregunta.',0,79,34,'#8ae9d0','center');
    text('ARQUITECTURA DE COMPUTADORES II',0,-112,21,'#9eb5d0','center');c.restore();
    const accent=c.createLinearGradient(725,0,1195,0);
    colors.forEach((color,k)=>accent.addColorStop(k/3,color));
    c.save();c.globalAlpha*=ease((s-.8)/1.2);line(960-235*reveal,662,960+235*reveal,662,accent,2);
    text('HECHO POR',960,710,18,'#97afc9','center');
    text('Jian Zheng · Fabricio Gonzalez',960,754,29,'#e0eafa','center');
    text('Melissa Vazquez · Jafet Diaz',960,797,29,'#e0eafa','center');c.restore();
  }
  const renderers={intro,question:s=>queue(s,s>=4),fine:s=>scheduling(s,true),coarse:s=>scheduling(s,false),hardware,dl,learn,matrix,dlResult:s=>chart('dl',s),nbody,gravity,barrier,nbodyResult:s=>chart('nbody',s),ray,samples,tiles:s=>tileWork(s),rayResult:s=>chart('ray',s),radar,phase,sentinelTiles:s=>tileWork(s,true),sentinelResult:s=>chart('sentinel',s),memory,evidence,compare,ending,thanks};
  function stamp(value){return `${Math.floor(value/60)}:${String(Math.floor(value%60)).padStart(2,'0')}`;}
  function current(){return film.shots.find(shot=>t>=shot.start&&t<shot.end)||film.shots[film.shots.length-1];}
  function draw() {
    const shot=current(),s=t-shot.start,key=shot.chapter,color=key==='Deep Learning'?colors[0]:key==='N-body'?colors[1]:key==='Ray Tracing'?colors[2]:key==='Sentinel-1'?colors[3]:'#b8cdf0';
    const bg=c.createLinearGradient(0,0,1920,1080);bg.addColorStop(0,'#060c17');bg.addColorStop(1,'#13283e');c.fillStyle=bg;c.fillRect(0,0,1920,1080);
    // Subtle moving background field; it never encodes measured performance.
    for(let i=0;i<65;i++){const x=fract(i*.618034)*1920,y=fract(i*.41421)*1000;dot(x,y,1.2,'#ffffff16');}
    glow(1620,250,550,color+'0c');
    const finalCard=shot.kind==='thanks';
    const transition=finalCard?ease(s/1.2):Math.min(ease(s/.65),ease((shot.duration-s)/.45));
    c.save();c.globalAlpha=transition;c.translate(0,(1-transition)*20);
    if(!finalCard){
      text('ARQUITECTURA DE COMPUTADORES II  /  '+key.toUpperCase(),100,75,22,'#aabbd1');
      text(shot.title,100,164,62,color);text(shot.subtitle,100,219,29,'#dbe8fa');
    }
    renderers[shot.kind](s);c.restore();
    if(!finalCard){
    c.fillStyle='#050c16';c.fillRect(0,895,1920,185);
    if(subtitles){const caption=shot.captions.find(item=>t<item.end)||shot.captions[shot.captions.length-1];wrap(caption.text,125,942,1670,29,'#edf3ff',41);}
    else{text('CUATRO PROBLEMAS · CINCO MODELOS · UNA COMPARACIÓN EXPERIMENTAL',100,970,24,'#91a8c5');}
    text(shot.kind==='memory'?'OBSERVACIÓN DEL GRUPO · FLUJO CONCEPTUAL':shot.kind.includes('Result')||shot.kind==='compare'?'DATOS DEL PROYECTO':'ILUSTRACIÓN CONCEPTUAL',100,1050,18,'#8098b9');
    }
    $('scene').value=String(shot.start);$('seek').value=t;$('time').textContent=`${stamp(t)} / ${stamp(duration)}`;$('play').textContent=playing?'Pausar':'Reproducir';
    $('narration').textContent=shot.narration;$('speaker').textContent=shot.speaker;$('shot-title').textContent=shot.title;
    document.querySelectorAll('[data-start]').forEach(button=>button.setAttribute('aria-current',Number(button.dataset.start)===shot.start?'true':'false'));
  }
  function jump(value) {if(recording)return;t=Math.max(0,Math.min(duration,value));last=null;draw();}
  $('play').onclick=()=>{if(recording)return;if(t>=duration)t=0;playing=!playing;last=null;draw();};
  $('restart').onclick=()=>jump(0);
  $('scene').onchange=e=>jump(Number(e.target.value)+.7);
  $('seek').oninput=e=>jump(Number(e.target.value));
  $('subtitles').onchange=e=>{subtitles=e.target.checked;draw();};
  $('fullscreen').onclick=async()=>{try{await canvas.requestFullscreen();}catch{$('status').textContent='Usá F11 si tu navegador no admite pantalla completa.';}};
  document.addEventListener('keydown',e=>{if(/INPUT|SELECT|BUTTON|TEXTAREA/.test(e.target.tagName)||recording)return;if(e.code==='Space'){e.preventDefault();$('play').click();}if(e.code==='ArrowRight')jump(t+5);if(e.code==='ArrowLeft')jump(t-5);});
  // Exports come from the same timeline used on screen to avoid script drift.
  function download(data,type,name){const url=URL.createObjectURL(new Blob([data],{type})),a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),60000);}
  function srtTime(n){return `00:${String(Math.floor(n/60)).padStart(2,'0')}:${String(n%60).padStart(2,'0')},000`;}
  $('srt').onclick=()=>download(film.shots.flatMap(shot=>shot.captions).map((caption,i)=>`${i+1}\n${srtTime(caption.start)} --> ${srtTime(caption.end)}\n${caption.text}\n`).join('\n'),'text/plain;charset=utf-8','subtitulos.srt');
  $('script').onclick=()=>download(`# Guion · Cuatro mundos, una pregunta\n\nDuración: ${stamp(duration)}.\n\n`+film.shots.map(shot=>`## ${stamp(shot.start)}–${stamp(shot.end)} · ${shot.speaker} · ${shot.title}\n\n${shot.narration}\n`).join('\n'),'text/markdown;charset=utf-8','guion.md');
  let stopRecording=null;
  $('record').onclick=()=>{
    if(recording){if(stopRecording)stopRecording();return;}
    if(!canvas.captureStream||typeof MediaRecorder==='undefined'){$('status').textContent='Este navegador no permite exportar. Podés grabar la pantalla con OBS.';return;}
    const mime=['video/webm;codecs=vp9','video/webm;codecs=vp8','video/webm'].find(x=>MediaRecorder.isTypeSupported(x));
    if(!mime){$('status').textContent='WebM no está disponible. Probá Chrome o Firefox, o grabá con OBS.';return;}
    let stream,recorder;const chunks=[],controls=['play','restart','scene','seek','subtitles'];let failed=false;
    function restore(){recording=false;playing=false;stopRecording=null;controls.forEach(id=>$(id).disabled=false);document.querySelectorAll('[data-start]').forEach(b=>b.disabled=false);$('record').textContent='Exportar WebM · 60 fps';if(stream)stream.getTracks().forEach(track=>track.stop());draw();}
    try{
      // Choose the full film or just the current shot before disabling controls.
      const single=$('export-mode').value==='shot',shotNumber=film.shots.indexOf(current())+1,start=single?current().start:0,end=single?current().end:duration;
      jump(start);stream=canvas.captureStream(60);recorder=new MediaRecorder(stream,{mimeType:mime,videoBitsPerSecond:20000000});
      recorder.ondataavailable=e=>{if(e.data.size)chunks.push(e.data);};
      recorder.onerror=()=>{failed=true;restore();$('status').textContent='Falló la exportación. Usá una grabación de pantalla.';};
      recorder.onstop=()=>{restore();if(failed||!chunks.length)return;download(new Blob(chunks,{type:mime}),mime,`cuatro-problemas-${single?'escena-'+shotNumber:'completo'}-1080p60.webm`);$('status').textContent='Descarga lista. El video no incluye audio; añadí las voces del grupo en el editor.';};
      recording=true;playing=true;controls.forEach(id=>$(id).disabled=true);document.querySelectorAll('[data-start]').forEach(b=>b.disabled=true);last=null;recorder.start(1000);$('record').textContent='Terminar exportación';
      $('status').textContent=`Grabando ${end-start} segundos en Full HD a 60 fps solicitados. Mantené esta pestaña visible. Podés terminar antes con el mismo botón.`;
      stopRecording=()=>{if(recorder.state!=='inactive')recorder.stop();};
      const stop=()=>{if(!recording)return;if(t>=end){stopRecording();return;}requestAnimationFrame(stop);};requestAnimationFrame(stop);
    }catch{restore();$('status').textContent='No se pudo iniciar la exportación. Podés grabar la pantalla con OBS.';}
  };
  // Populate the scene picker and the clickable storyboard from the same source.
  $('scene').replaceChildren();
  $('seek').max=duration;
  film.shots.forEach(shot=>{
    const option=document.createElement('option');option.value=shot.start;option.textContent=`${stamp(shot.start)} · ${shot.title}`;$('scene').append(option);
    const b=document.createElement('button');b.dataset.start=shot.start;b.textContent=`${stamp(shot.start)}  ${shot.title}`;b.onclick=()=>jump(shot.start+.7);$('storyboard').append(b);
  });
  // Draw each browser animation frame; throttling to 30 would duplicate frames
  // in the 60 fps capture. Actual cadence depends on browser and hardware.
  function tick(now){const advance=playing&&!document.hidden;if(last!==null&&advance)t=Math.min(duration,t+(now-last)/1000);last=now;if(advance)draw();if(t>=duration&&playing){playing=false;draw();}requestAnimationFrame(tick);}
  window.videoPreview={seek:jump,get time(){return t;},get duration(){return duration;}};
  draw();requestAnimationFrame(tick);
})();
