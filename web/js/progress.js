// Nivel y medallas del usuario. Son las MISMAS reglas que usa la app de la
// consola (common/progress.c): si se cambia algo aqui, hay que cambiarlo alla.
(function () {
  "use strict";

  // Para pasar del nivel n al n+1 hacen falta 50*n puntos: 50, 100, 150...
  function level(points) {
    let lvl = 1, left = Math.max(0, points | 0);
    while (left >= 50 * lvl && lvl < 999) { left -= 50 * lvl; lvl++; }
    return { level: lvl, into: left, span: 50 * lvl };
  }

  // kind: de que numero depende. El id es tambien el nombre del dibujo (<id>.png).
  const BADGES = [
    { id: "primer_logro",   kind: "achievements", goal: 1,    name: "Primer brote",   how: "Desbloquea tu primer logro." },
    { id: "logros_10",      kind: "achievements", goal: 10,   name: "Coleccionista",  how: "Desbloquea 10 logros." },
    { id: "logros_50",      kind: "achievements", goal: 50,   name: "Cazatrofeos",    how: "Desbloquea 50 logros." },
    { id: "logros_200",     kind: "achievements", goal: 200,  name: "Leyenda",        how: "Desbloquea 200 logros." },
    { id: "primer_platino", kind: "platinums",    goal: 1,    name: "Primer platino", how: "Consigue todos los logros de un juego." },
    { id: "platinos_5",     kind: "platinums",    goal: 5,    name: "Completista",    how: "Platina 5 juegos." },
    { id: "juegos_3",       kind: "games",        goal: 3,    name: "Explorador",     how: "Consigue logros en 3 juegos distintos." },
    { id: "juegos_10",      kind: "games",        goal: 10,   name: "Trotamundos",    how: "Consigue logros en 10 juegos distintos." },
    { id: "puntos_100",     kind: "points",       goal: 100,  name: "Cien puntos",    how: "Llega a 100 puntos." },
    { id: "puntos_500",     kind: "points",       goal: 500,  name: "Quinientos",     how: "Llega a 500 puntos." },
    { id: "puntos_2000",    kind: "points",       goal: 2000, name: "Imparable",      how: "Llega a 2000 puntos." },
  ];

  // stats: { achievements, platinums, games, points }
  function badges(stats) {
    return BADGES.map((b) => {
      const value = Number(stats[b.kind] || 0);
      return { ...b, value, earned: value >= b.goal };
    });
  }

  window.GreenyProgress = { level, badges };
})();
