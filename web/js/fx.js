// Animaciones de la web: aparecer al bajar, numeros que cuentan y barras que se llenan.
// Ligeras: un solo IntersectionObserver, nada corre si no se ve, y si la persona pidio
// menos movimiento en su sistema no se anima nada.
(function () {
  "use strict";
  const calm = window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  const seen = new WeakSet();

  function countUp(el) {
    const to = Number(el.dataset.count) || 0;
    const fmt = (n) => Math.round(n).toLocaleString((window.GreenyI18n && window.GreenyI18n.lang) || "es");
    if (calm || to < 2) { el.textContent = fmt(to); return; }
    const t0 = performance.now(), dur = Math.min(1400, 500 + to * 6);
    const tick = (t) => {
      const k = Math.min(1, (t - t0) / dur);
      el.textContent = fmt(to * (1 - Math.pow(1 - k, 3)));
      if (k < 1) requestAnimationFrame(tick);
    };
    requestAnimationFrame(tick);
  }

  const io = "IntersectionObserver" in window ? new IntersectionObserver((entries) => {
    for (const e of entries) {
      if (!e.isIntersecting) continue;
      const el = e.target;
      io.unobserve(el);
      el.classList.add("in");
      if (el.dataset.count != null) countUp(el);
    }
  }, { rootMargin: "0px 0px -6% 0px" }) : null;

  // Se llama despues de cada pantalla nueva.
  function scan(root) {
    const nodes = root.querySelectorAll(".card, h2, .reveal, [data-count], .bar");
    let i = 0;
    for (const el of nodes) {
      if (seen.has(el)) continue;
      seen.add(el);
      if (!io || calm) { el.classList.add("in"); if (el.dataset.count != null) countUp(el); continue; }
      el.classList.add("rv");
      el.style.setProperty("--rv-delay", Math.min(i++, 8) * 40 + "ms"); // los primeros entran en cascada
      io.observe(el);
    }
  }

  window.GreenyFx = { scan };
})();
