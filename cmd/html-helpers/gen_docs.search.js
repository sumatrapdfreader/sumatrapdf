function driver() {
  let q =
    "//table[contains(@class,'collection-content')]/tbody/tr[not(./td/input)]";
  let rows = getElementByXpath(q);
  // console.log("rows:", rows.length);
  let lists = [];
  let inputs = [];
  let selectors = ["input#cmd_ids", "input#key_sht", "input#cmd_plt"];
  selectors.forEach((sel, idx) => {
    let el = document.querySelector(sel);
    inputs[idx] = el;
    setEvent(el, tableFilter);
  });
  for (let i = 1; i <= selectors.length; i++) {
    q =
      "//table[contains(@class,'collection-content')]/tbody/tr/td[(not(./input))][position()=" +
      i +
      "]";
    let els = getElementByXpath(q);
    els = els.map((x) => x.innerText);
    lists[i - 1] = els;
  }
  lists[1] = lists[1].map((x) => x.replace(/(?:(?<!\+)|(?<=\+\+))\,/g, "")); //removing commas b/w shortcuts

  /**
   * @param {HTMLElement} el
   */
  function hideEl(el) {
    el.setAttribute("style", "display: none;");
  }

  /**
   * @param {HTMLElement} el
   */
  function showEl(el) {
    el.removeAttribute("style");
  }

  /**
   * @param {HTMLElement} el
   */
  function isVisible(el) {
    return !el.hasAttribute("style");
  }

  // called when any of the 3 search input fields changes
  // hides tr rows that don't match search query
  function tableFilter() {
    let regexs = [
      getRegex_cmdids(inputs[0]),
      getRegex_keysht(inputs[1]),
      getRegex_cmdplt(inputs[2]),
    ];
    rows.forEach(hideEl);
    let shortlist = new Array(rows.length).fill(undefined);
    regexs.forEach((regex, list_index) => {
      if (!!regex)
        lists[list_index].forEach((item, row_index) => {
          if (shortlist[row_index] === undefined)
            shortlist[row_index] = regex.test(item);
          else if (shortlist[row_index])
            shortlist[row_index] = regex.test(item);
        });
    });
    if (!regexs.some((x) => !!x)) {
      rows.forEach(showEl);
    } else {
      shortlist.forEach((flag, index) => {
        if (flag) {
          showEl(rows[index]);
        }
      });
    }

    let qTables = "//table[contains(@class,'collection-content')]";
    let tables = getElementByXpath(qTables);
    // console.log("tables:", tables);
    for (let table of tables) {
      let h = table.previousSibling;
      if (h.nodeName == "#text") {
        h = h.previousSibling;
      }
      let isPrevHdr = h.nodeName === "H2" || h.nodeName === "H3";
      if (!isPrevHdr) {
        console.log("h.nodeName is not header:", h.nodeName);
        continue;
      }
      let rows = table.querySelectorAll("tbody > tr");
      let nVisible = 0;
      for (let row of rows) {
        if (isVisible(row)) {
          nVisible++;
        }
      }
      if (nVisible > 0) {
        showEl(table);
        showEl(h);
      } else {
        hideEl(table);
        hideEl(h);
      }
    }
  }
}

function setEvent(target, callback) {
  target.addEventListener("keyup", callback);
}

function getElementByXpath(xpathToExecute) {
  let result = [];
  let snapshotNodes = document.evaluate(
    xpathToExecute,
    document,
    null,
    XPathResult.ORDERED_NODE_SNAPSHOT_TYPE,
    null
  );
  for (let i = 0; i < snapshotNodes.snapshotLength; i++)
    result.push(snapshotNodes.snapshotItem(i));
  return result;
}

function getRegex_cmdids(ele) {
  let ip_val = ele.value.replace(/([^\w\s])/g, "").replace(/\s+$/, "");
  if (ip_val.length == 0) return false;
  return new RegExp(ip_val.replace(/\s+(\w+)/g, "(?=.*$1)"), "i");
}

function getRegex_keysht(ele) {
  let ip_val = ele.value.replace(/\s+$/, "");
  if (ip_val.length == 0) return false;
  return new RegExp(
    "(?:(?=\\W)(?<=\\w)|(?<!\\w))(" +
      ip_val
        .replace(/([^\w\s])/g, "\\$1")
        .replace(/([^\s]+)/g, "($1)")
        .replace(/\s+/g, "|") +
      ")(?:(?<=\\W)(?=\\w)|(?!\\w))",
    "i"
  );
}

function getRegex_cmdplt(ele) {
  let ip_val = ele.value.replace(/\s+$/, "");
  if (ip_val.length == 0) return false;
  return new RegExp(
    "(?:(?=\\W)(?<=\\w)|(?<!\\w))" +
      ip_val
        .replace(/([^\w\s])/g, "\\$1")
        .replace(/\s+([\w\W]+)/g, "(?=.*\\b$1)"),
    "i"
  );
}
driver();
