// Copy buttons for fenced code blocks (.sum-code-copy-btn inside .code-block).
// Uses document-level event delegation: the in-app manual injects page HTML
// after load (gen_docs.render.js), so binding on DOMContentLoaded alone never
// sees the buttons (issue #5855).

function copyTextViaTextarea(text) {
  return new Promise(function (resolve, reject) {
    const ta = document.createElement("textarea");
    ta.value = text;
    ta.setAttribute("readonly", "");
    ta.style.position = "fixed";
    ta.style.left = "-9999px";
    ta.style.top = "0";
    document.body.appendChild(ta);
    ta.focus();
    ta.select();
    try {
      const ok = document.execCommand("copy");
      document.body.removeChild(ta);
      if (ok) {
        resolve();
      } else {
        reject(new Error("execCommand copy failed"));
      }
    } catch (e) {
      if (ta.parentNode) {
        document.body.removeChild(ta);
      }
      reject(e);
    }
  });
}

function copyTextToClipboard(text) {
  if (navigator.clipboard && window.isSecureContext && navigator.clipboard.writeText) {
    return navigator.clipboard.writeText(text).catch(function () {
      return copyTextViaTextarea(text);
    });
  }
  return copyTextViaTextarea(text);
}

function flashCopyButton(btn, ok) {
  const label = btn.getAttribute("data-copy-label") || btn.textContent || "Copy";
  btn.setAttribute("data-copy-label", label);
  if (ok) {
    btn.textContent = "Copied!";
    btn.classList.add("copied");
  } else {
    btn.textContent = "Failed";
    btn.classList.remove("copied");
  }
  setTimeout(function () {
    btn.textContent = label;
    btn.classList.remove("copied");
  }, 1500);
}

function onCodeCopyClick(e) {
  const btn = e.target && e.target.closest ? e.target.closest(".sum-code-copy-btn") : null;
  if (!btn) {
    return;
  }
  const block = btn.closest(".code-block");
  if (!block) {
    return;
  }
  const code = block.querySelector("code");
  if (!code) {
    return;
  }
  e.preventDefault();
  const text = code.innerText || code.textContent || "";
  copyTextToClipboard(text)
    .then(function () {
      flashCopyButton(btn, true);
    })
    .catch(function () {
      flashCopyButton(btn, false);
    });
}

function initCodeCopyButtons() {
  if (document.documentElement.dataset.codeCopyBound === "1") {
    return;
  }
  document.documentElement.dataset.codeCopyBound = "1";
  document.addEventListener("click", onCodeCopyClick);
}

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", initCodeCopyButtons);
} else {
  initCodeCopyButtons();
}

// Available for post-render hooks (optional; delegation already covers new buttons).
if (typeof window !== "undefined") {
  window.initCodeCopyButtons = initCodeCopyButtons;
}
