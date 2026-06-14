function copyTextToClipboard(text) {
  if (navigator.clipboard && window.isSecureContext) {
    return navigator.clipboard.writeText(text);
  }
  const ta = document.createElement("textarea");
  ta.value = text;
  ta.style.position = "fixed";
  ta.style.left = "-9999px";
  document.body.appendChild(ta);
  ta.select();
  document.execCommand("copy");
  document.body.removeChild(ta);
  return Promise.resolve();
}

function initCodeCopyButtons() {
  for (const btn of document.querySelectorAll(".sum-code-copy-btn")) {
    btn.addEventListener("click", async () => {
      const block = btn.closest(".code-block");
      if (!block) {
        return;
      }
      const code = block.querySelector("code");
      if (!code) {
        return;
      }
      const label = btn.textContent;
      try {
        await copyTextToClipboard(code.textContent);
        btn.textContent = "Copied!";
        btn.classList.add("copied");
        setTimeout(() => {
          btn.textContent = label;
          btn.classList.remove("copied");
        }, 1500);
      } catch (_e) {
        btn.textContent = "Failed";
        setTimeout(() => {
          btn.textContent = label;
        }, 1500);
      }
    });
  }
}

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", initCodeCopyButtons);
} else {
  initCodeCopyButtons();
}