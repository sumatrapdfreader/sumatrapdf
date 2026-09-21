(function (w) {
  var KEY = "sumatra-theme";

  function normalize(p) {
    if (p === "light" || p === "dark" || p === "system") return p;
    return "system";
  }

  // in the in-app manual the choice lives in SumatraPDF's HelpTheme setting:
  // the app announces it before this script runs and is told about changes
  function inApp() {
    return typeof w.SumatraManualTheme === "string" && !!w.__sumatra__;
  }

  function pref() {
    if (inApp()) return normalize(w.SumatraManualTheme);
    try {
      return normalize(localStorage.getItem(KEY) || "system");
    } catch (e) {
      return "system";
    }
  }

  function resolved(p) {
    p = normalize(p);
    if (p === "light" || p === "dark") return p;
    // in the in-app manual "system" follows SumatraPDF's own theme, which the
    // app announces before this script runs (ManualInjectThemeCss)
    var app = w.SumatraAppTheme;
    if (app === "light" || app === "dark") return app;
    return w.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
  }

  function syncButtons(p) {
    document.querySelectorAll("[data-theme-set]").forEach(function (btn) {
      var on = btn.getAttribute("data-theme-set") === p;
      btn.setAttribute("aria-pressed", on ? "true" : "false");
      btn.classList.toggle("theme-opt-active", on);
    });
  }

  function apply(p) {
    p = normalize(p || pref());
    var r = resolved(p);
    document.documentElement.dataset.theme = r;
    document.documentElement.dataset.themePref = p;
    syncButtons(p);
  }

  function set(p) {
    p = normalize(p);
    if (inApp()) {
      w.SumatraManualTheme = p;
      w.__sumatra__.notify("manualTheme", p);
    } else {
      try {
        localStorage.setItem(KEY, p);
      } catch (e) {}
    }
    apply(p);
  }

  w.SumatraTheme = { key: KEY, pref: pref, resolved: resolved, apply: apply, set: set };

  apply();

  try {
    w.matchMedia("(prefers-color-scheme: dark)").addEventListener("change", function () {
      if (pref() === "system") apply("system");
    });
  } catch (e) {}

  function lockSwitch(sw) {
    if (!sw) return;
    sw.classList.add("is-locked");
    var armed = false;
    function pointerInside(e) {
      var r = sw.getBoundingClientRect();
      return e.clientX >= r.left && e.clientX <= r.right && e.clientY >= r.top && e.clientY <= r.bottom;
    }
    function unlock(e) {
      if (!armed) return;
      if (e.type === "pointermove" && pointerInside(e)) return;
      sw.classList.remove("is-locked");
      sw.removeEventListener("pointerleave", unlock);
      document.removeEventListener("pointermove", unlock, true);
    }
    sw.addEventListener("pointerleave", unlock);
    document.addEventListener("pointermove", unlock, true);
    w.setTimeout(function () {
      armed = true;
    }, 0);
  }

  document.addEventListener("DOMContentLoaded", function () {
    apply();
    document.querySelectorAll("[data-theme-set]").forEach(function (btn) {
      btn.addEventListener("click", function () {
        set(btn.getAttribute("data-theme-set"));
        lockSwitch(btn.closest(".theme-switch"));
        btn.blur();
      });
    });
  });
})(window);
