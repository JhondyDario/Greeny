// Lee un archivo de logros de Greeny (el mismo .txt que va en la SD) y saca lo
// que necesita la web: nombre, version y la lista de logros con sus textos.
// No valida las condiciones `when`; para eso esta test/lgcheck.
(function () {
  "use strict";

  function parseDefinition(text) {
    const game = { name: "", version: "", build: "", achievements: [] };
    const seen = new Set();
    let cur = null;
    const lines = text.replace(/^﻿/, "").split(/\r?\n/);

    for (let n = 0; n < lines.length; n++) {
      const line = lines[n].trim();
      if (!line || line.startsWith("#")) continue;
      const where = "línea " + (n + 1) + ": ";

      if (line.startsWith("[")) {
        if (!line.endsWith("]")) throw new Error(where + "sección mal cerrada");
        const id = line.slice(1, -1).trim();
        if (!/^[A-Za-z0-9_-]{1,31}$/.test(id)) throw new Error(where + "id inválido");
        if (seen.has(id)) throw new Error(where + "id repetido: " + id);
        seen.add(id);
        cur = { id, name: id, desc: "", name_en: null, desc_en: null, points: 10, conditions: 0 };
        game.achievements.push(cur);
        continue;
      }

      const eq = line.indexOf("=");
      if (eq < 0) throw new Error(where + "se esperaba clave = valor");
      const key = line.slice(0, eq).trim();
      const val = line.slice(eq + 1).trim();

      if (!cur) {
        if (key === "game") game.name = val;
        else if (key === "version") game.version = val;
        else if (key === "build") game.build = val.toUpperCase();
        continue; // debug y scan no le importan a la web
      }
      if (key === "name") cur.name = val;
      else if (key === "desc") cur.desc = val;
      else if (key === "name.en") cur.name_en = val;
      else if (key === "desc.en") cur.desc_en = val;
      else if (key === "when") cur.conditions++;
      else if (key === "points") {
        const p = Number(val);
        if (!Number.isInteger(p) || p < 0 || p > 1000) throw new Error(where + "points inválido");
        cur.points = p;
      }
    }

    if (!game.achievements.length) throw new Error("el archivo no define ningún logro");
    const empty = game.achievements.find((a) => a.conditions === 0);
    if (empty) throw new Error("el logro [" + empty.id + "] no tiene ninguna condición when");
    return game;
  }

  window.GreenyDefs = { parseDefinition };
})();
