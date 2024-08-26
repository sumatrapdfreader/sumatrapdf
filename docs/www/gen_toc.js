function getAllHeaders() {
  return Array.from(document.querySelectorAll("h1, h2, h3, h4, h5, h6"));
}

function removeHash(str) {
  return str.replace(/#$/, "");
}

class TocItem {
  text = "";
  hLevel = 0;
  nesting = 0;
  element;
}

function buildTocItems() {
  let allHdrs = getAllHeaders();
  let res = [];
  let el = document.getElementsByClassName("breadcrumbs")[0];
  if (el) {
    let h = new TocItem();
    h.text = "Home";
    h.element = el;
    res.push(h);
  }
  for (let el of allHdrs) {
    /** @type {string} */
    let text = el.innerText.trim();
    text = removeHash(text);
    text = text.trim();
    let hLevel = parseInt(el.tagName[1]);
    let h = new TocItem();
    h.text = text;
    h.hLevel = hLevel;
    h.nesting = 0;
    h.element = el;
    res.push(h);
  }
  return res;
}

function fixNesting(hdrs) {
  let n = hdrs.length;
  for (let i = 0; i < n; i++) {
    let h = hdrs[i];
    if (i == 0) {
      h.nesting = 0;
    } else {
      h.nesting = h.hLevel - 1;
    }
    // console.log(`${h.hLevel} => ${h.nesting}`);
  }
}

function genTocMini(items) {
  let tmp = "";
  let t = `<div class="toc-item-mini toc-light">▃</div>`;
  for (let i = 0; i < items.length; i++) {
    tmp += t;
  }
  return `<div class="toc-mini">` + tmp + `</div>`;
}

function genTocList(items) {
  let tmp = "";
  let t = `<div title="{title}" class="toc-item toc-trunc {ind}" onclick=tocGoTo({n})>{text}</div>`;
  let n = 0;
  for (let h of items) {
    let s = t;
    s = s.replace("{n}", n);
    let ind = "toc-ind-" + h.nesting;
    s = s.replace("{ind}", ind);
    s = s.replace("{text}", h.text);
    s = s.replace("{title}", h.text);
    tmp += s;
    n++;
  }
  return `<div class="toc-list">` + tmp + `</div>`;
}

/**
 * @param {HTMLElement} el
 */
function highlightElement(el) {
  let tempBgColor = "yellow";
  let origCol = el.style.backgroundColor;
  if (origCol === tempBgColor) {
    return;
  }
  el.style.backgroundColor = tempBgColor;
  setTimeout(() => {
    el.style.backgroundColor = origCol;
  }, 1000);
}

let tocItems = [];
function tocGoTo(n) {
  let el = tocItems[n].element;
  let y = el.getBoundingClientRect().top + window.scrollY;
  let offY = 12;
  // for website: account for nav bar at the top covering top of page
  let navEl = document.getElementsByClassName("nav")[0];
  if (navEl) {
    offY = navEl.clientHeight;
  }
  y -= offY;
  window.scrollTo({
    top: y,
  });
  highlightElement(el);
  // the above scrollTo() triggers updateClosestToc() which might
  // not be accurate so we set the exact selected after a small delay
  setTimeout(() => {
    showSelectedTocItem(n);
  }, 100);
}

function genToc() {
  tocItems = buildTocItems();
  fixNesting(tocItems);
  const container = document.createElement("div");
  container.className = "toc-wrapper";
  let s = genTocMini(tocItems);
  let s2 = genTocList(tocItems);
  container.innerHTML = s + s2;
  document.body.appendChild(container);
}

function showSelectedTocItem(elIdx) {
  // make toc-mini-item black for closest element
  let els = document.querySelectorAll(".toc-item-mini");
  let cls = "toc-light";
  for (let i = 0; i < els.length; i++) {
    let el = els[i];
    if (i == elIdx) {
      el.classList.remove(cls);
    } else {
      el.classList.add(cls);
    }
  }

  // make toc-item bold for closest element
  els = document.querySelectorAll(".toc-item");
  cls = "toc-bold";
  for (let i = 0; i < els.length; i++) {
    let el = els[i];
    if (i == elIdx) {
      el.classList.add(cls);
    } else {
      el.classList.remove(cls);
    }
  }
}

function updateClosestToc() {
  let closestIdx = -1;
  let closestDistance = Infinity;

  for (let i = 0; i < tocItems.length; i++) {
    let tocItem = tocItems[i];
    const rect = tocItem.element.getBoundingClientRect();
    const distanceFromTop = Math.abs(rect.top);
    if (
      distanceFromTop < closestDistance &&
      rect.bottom > 0 &&
      rect.top < window.innerHeight
    ) {
      closestDistance = distanceFromTop;
      closestIdx = i;
    }
  }
  if (closestIdx >= 0) {
    showSelectedTocItem(closestIdx);
  }
}

window.addEventListener("scroll", updateClosestToc);

genToc();
updateClosestToc();
