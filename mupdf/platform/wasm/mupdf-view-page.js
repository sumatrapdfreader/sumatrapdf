// Copyright (C) 2004-2022 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

/* eslint-disable no-unused-vars */

class MupdfPageViewer {
	constructor(worker, pageNumber, defaultSize, dpi) {
		this.worker = worker;
		this.pageNumber = pageNumber;
		this.size = defaultSize;
		this.sizeIsDefault = true;

		const rootNode = document.createElement("div");
		rootNode.classList.add("page");

		const canvasNode = document.createElement("canvas");
		rootNode.appendChild(canvasNode);

		const anchor = document.createElement("a");
		anchor.classList.add("anchor");
		// TODO - document the "+ 1" better
		anchor.id = "page" + (pageNumber + 1);
		rootNode.appendChild(anchor);
		rootNode.pageNumber = pageNumber;

		this.rootNode = rootNode;
		this.canvasNode = canvasNode;
		this.canvasCtx = canvasNode.getContext("2d");
		this._updateSize(dpi);

		this.renderPromise = null;
		this.queuedRenderArgs = null;
		this.renderCookie = null;

		this.textNode = null;
		this.textPromise = null;
		this.textResultObject = null;

		this.linksNode = null;
		this.linksPromise = null;
		this.linksResultObject = null;

		this.searchHitsNode = null;
		this.searchPromise = null;
		this.searchResultObject = null;
		this.lastSearchNeedle = null;
		this.searchNeedle = null;
	}

	// TODO - move searchNeedle out
	render(dpi, searchNeedle) {
		// TODO - error handling
		this._loadPageImg({ dpi });
		this._loadPageText(dpi);
		this._loadPageLinks(dpi);
		this._loadPageSearch(dpi, searchNeedle);
	}

	cancelRender() {
		// TODO - use promise cancelling
		if (this.renderCookie != null) {
			const int32pointer = this.renderCookie >> 2;
			const wasmMemoryView32 = new Int32Array(this.worker.wasmMemory);
			wasmMemoryView32[int32pointer] = 1;
		}
	}

	// TODO - update child nodes
	setZoom(zoomLevel) {
		const dpi = (zoomLevel * 96 / 100) | 0;

		this._updateSize(dpi);
	}

	setSearchNeedle(searchNeedle = null) {
		this.searchNeedle = searchNeedle;
	}

	clear() {
		this.cancelRender();

		this.textNode?.remove();
		this.linksNode?.remove();
		this.searchHitsNode?.remove();

		// TODO - use promise cancelling
		this.renderPromise = null;
		this.textPromise = null;
		this.linksPromise = null;
		this.searchPromise = null;

		this.renderPromise = null;
		this.queuedRenderArgs = null;
		this.renderCookie = null;

		this.textNode = null;
		this.textPromise = null;
		this.textResultObject = null;

		this.linksNode = null;
		this.linksPromise = null;
		this.linksResultObject = null;

		this.searchHitsNode = null;
		this.searchPromise = null;
		this.searchResultObject = null;
		this.lastSearchNeedle = null;
		this.searchNeedle = null;

		this.mouseIsPressed = false;
	}

	// TODO - this is destructive and makes other method get null ref errors
	showError(functionName, error) {
		console.error(`mupdf.${functionName}: ${error.message}:\n${error.stack}`);

		let div = document.createElement("div");
		div.classList.add("error");
		div.textContent = error.name + ": " + error.message;
		this.clear();
		this.rootNode.replaceChildren(div);
	}

	async mouseDown(event, dpi) {
		let { x, y } = this._getLocalCoords(event.clientX, event.clientY);
		// TODO - remove "+ 1"
		let changed = await this.worker.mouseDownOnPage(this.pageNumber + 1, dpi * devicePixelRatio, x, y);
		this.mouseIsPressed = true;
		if (changed) {
			this._invalidatePageImg();
			this._loadPageImg({ dpi });
		}
	}

	async mouseMove(event, dpi) {
		let { x, y } = this._getLocalCoords(event.clientX, event.clientY);
		let changed;
		// TODO - handle multiple buttons
		// see https://developer.mozilla.org/en-US/docs/Web/API/MouseEvent/buttons
		if (this.mouseIsPressed) {
			if (event.buttons == 0) {
				// In case we missed an onmouseup event outside of the frame
				this.mouseIsPressed = false;
				// TODO - remove "+ 1"
				changed = await this.worker.mouseUpOnPage(this.pageNumber + 1, dpi * devicePixelRatio, x, y);
			}
			else {
				// TODO - remove "+ 1"
				changed = await this.worker.mouseDragOnPage(this.pageNumber + 1, dpi * devicePixelRatio, x, y);
			}
		}
		else {
			// TODO - remove "+ 1"
			changed = await this.worker.mouseMoveOnPage(this.pageNumber + 1, dpi * devicePixelRatio, x, y);
		}
		if (changed) {
			this._invalidatePageImg();
			this._loadPageImg({ dpi });
		}
	}

	async mouseUp(event, dpi) {
		let { x, y } = this._getLocalCoords(event.clientX, event.clientY);
		this.mouseIsPressed = false;
		// TODO - remove "+ 1"
		let changed = await this.worker.mouseUpOnPage(this.pageNumber + 1, dpi * devicePixelRatio, x, y);
		if (changed) {
			this._invalidatePageImg();
			this._loadPageImg({ dpi });
		}
	}

	// --- INTERNAL METHODS ---

	// TODO - remove dpi param
	_updateSize(dpi) {
		// We use the `foo | 0` notation to convert dimensions to integers.
		// This matches the conversion done in `mupdf.js` when `Pixmap.withBbox`
		// calls `libmupdf._wasm_new_pixmap_with_bbox`.
		this.rootNode.style.width = (this.size.width * dpi / 72 | 0) + "px";
		this.rootNode.style.height = (this.size.height * dpi / 72 | 0) + "px";
		this.canvasNode.style.width = (this.size.width * dpi / 72 | 0) + "px";
		this.canvasNode.style.height = (this.size.height * dpi / 72 | 0) + "px";
	}

	async _loadPageImg(renderArgs) {
		if (this.renderPromise != null || this.renderIsOngoing) {
			// If a render is ongoing, we mark the current arguments as queued
			// to be processed when the render ends.
			// This also erases any previous queued render arguments.
			this.queuedRenderArgs = renderArgs;
			return;
		}
		if (this.canvasNode?.renderArgs != null) {
			// If the current image node was rendered with the same arguments
			// we skip the render.
			if (renderArgs.dpi === this.canvasNode.renderArgs.dpi)
				return;
		}

		let { dpi } = renderArgs;

		try {
			// FIXME - find better system for skipping duplicate renders
			this.renderIsOngoing = true;

			if (this.sizeIsDefault) {
				// TODO - remove "+ 1"
				this.size = await this.worker.getPageSize(this.pageNumber + 1);
				this.sizeIsDefault = false;
				this._updateSize(dpi);
			}
			this.renderCookie = await this.worker.createCookie();
			// TODO - remove "+ 1"
			this.renderPromise = this.worker.drawPageAsPixmap(this.pageNumber + 1, dpi * devicePixelRatio, this.renderCookie);
			let imageData = await this.renderPromise;

			// if render was aborted, return early
			if (imageData == null)
				return;

			this.canvasNode.renderArgs = renderArgs;
			this.canvasNode.width = imageData.width;
			this.canvasNode.height = imageData.height;
			this.canvasCtx.putImageData(imageData, 0, 0);

		}
		catch (error) {
			this.showError("_loadPageImg", error);
		}
		finally {
			this.worker.deleteCookie(this.renderCookie);
			this.renderCookie = null;
			this.renderPromise = null;
			this.renderIsOngoing = false;
		}

		if (this.queuedRenderArgs != null) {
			// TODO - Error handling
			this._loadPageImg(this.queuedRenderArgs);
			this.queuedRenderArgs = null;
		}
	}

	_invalidatePageImg() {
		if (this.canvasNode)
			this.canvasNode.renderArgs = null;
	}

	// TODO - replace "dpi" with "scale"?
	async _loadPageText(dpi) {
		// TODO - Disable text when editing (conditions to be figured out)
		if (this.textNode != null && dpi === this.textNode.dpi) {
			// Text was already rendered at the right scale, nothing to be done
			return;
		}
		if (this.textResultObject) {
			// Text was already returned, just needs to be rescaled
			this._applyPageText(this.textResultObject, dpi);
			return;
		}

		let textNode = document.createElement("div");
		textNode.classList.add("text");

		this.textNode?.remove();
		this.textNode = textNode;
		this.rootNode.appendChild(textNode);

		try {
			// TODO - remove "+ 1"
			this.textPromise = this.worker.getPageText(this.pageNumber + 1);

			this.textResultObject = await this.textPromise;
			this._applyPageText(this.textResultObject, dpi);
		}
		catch (error) {
			this.showError("_loadPageText", error);
		}
		finally {
			this.textPromise = null;
		}
	}

	_applyPageText(textResultObject, dpi) {
		console.log("PAGE TEXT:", textResultObject);
		this.textNode.dpi = dpi;
		let nodes = [];
		let pdf_w = [];
		let html_w = [];
		let text_len = [];
		let scale = dpi / 72;
		this.textNode.replaceChildren();
		for (let block of textResultObject.blocks) {
			if (block.type === "text") {
				for (let line of block.lines) {
					let text = document.createElement("span");
					text.style.left = (line.bbox.x * scale) + "px";
					text.style.top = ((line.y - line.font.size * 0.8) * scale) + "px";
					text.style.height = (line.bbox.h * scale) + "px";
					text.style.fontSize = (line.font.size * scale) + "px";
					text.style.fontFamily = line.font.family;
					text.style.fontWeight = line.font.weight;
					text.style.fontStyle = line.font.style;
					text.textContent = line.text;
					this.textNode.appendChild(text);
					nodes.push(text);
					pdf_w.push(line.bbox.w);
					text_len.push(line.text.length-1);
				}
			}
		}
		for (let i = 0; i < nodes.length; ++i) {
			if (text_len[i] > 0)
				html_w[i] = nodes[i].clientWidth;
		}
		for (let i = 0; i < nodes.length; ++i) {
			if (text_len[i] > 0)
				nodes[i].style.letterSpacing = ((pdf_w[i] - html_w[i]) / text_len[i]) + "px";
		}
	}

	async _loadPageLinks(dpi) {
		if (this.linksNode != null && dpi === this.linksNode.dpi) {
			// Links were already rendered at the right scale, nothing to be done
			return;
		}
		if (this.linksResultObject) {
			// Links were already returned, just need to be rescaled
			this._applyPageLinks(this.linksResultObject, dpi);
			return;
		}

		let linksNode = document.createElement("div");
		linksNode.classList.add("links");

		// TODO - Figure out node order
		this.linksNode?.remove();
		this.linksNode = linksNode;
		this.rootNode.appendChild(linksNode);

		try {
			// TODO - remove "+ 1"
			this.linksPromise = this.worker.getPageLinks(this.pageNumber + 1);

			this.linksResultObject = await this.linksPromise;
			this._applyPageLinks(this.linksResultObject, dpi);
		}
		catch (error) {
			this.showError("_loadPageLinks", error);
		}
		finally {
			this.linksPromise = null;
		}
	}

	_applyPageLinks(linksResultObject, dpi) {
		let scale = dpi / 72;
		this.linksNode.dpi = dpi;
		this.linksNode.replaceChildren();
		for (let link of linksResultObject) {
			let a = document.createElement("a");
			a.href = link.href;
			a.style.left = (link.x * scale) + "px";
			a.style.top = (link.y * scale) + "px";
			a.style.width = (link.w * scale) + "px";
			a.style.height = (link.h * scale) + "px";
			this.linksNode.appendChild(a);
		}
	}

	async _loadPageSearch(dpi, searchNeedle) {
		if (this.searchHitsNode != null && dpi === this.searchHitsNode.dpi && searchNeedle == this.searchHitsNode.searchNeedle) {
			// Search results were already rendered at the right scale, nothing to be done
			return;
		}
		if (this.searchResultObject && searchNeedle == this.searchHitsNode.searchNeedle) {
			// Search results were already returned, just need to be rescaled
			this._applyPageSearch(this.searchResultObject, dpi);
			return;
		}

		// TODO - cancel previous load

		let searchHitsNode = document.createElement("div");
		searchHitsNode.classList.add("searchHitList");
		this.searchHitsNode?.remove();
		this.searchHitsNode = searchHitsNode;
		this.rootNode.appendChild(searchHitsNode);

		this.searchNeedle = searchNeedle ?? "";

		try {
			if (this.searchNeedle !== "") {
				// TODO - remove "+ 1"
				console.log("SEARCH", this.pageNumber + 1, JSON.stringify(this.searchNeedle));
				this.searchPromise = this.worker.search(this.pageNumber + 1, this.searchNeedle);
				this.searchResultObject = await this.searchPromise;
			}
			else {
				this.searchResultObject = [];
			}

			this._applyPageSearch(this.searchResultObject, searchNeedle, dpi);
		}
		catch (error) {
			this.showError("_loadPageSearch", error);
		}
		finally {
			this.searchPromise = null;
		}
	}

	_applyPageSearch(searchResultObject, searchNeedle, dpi) {
		let scale = dpi / 72;
		this.searchHitsNode.searchNeedle = searchNeedle;
		this.searchHitsNode.dpi = dpi;
		this.searchHitsNode.replaceChildren();
		for (let bbox of searchResultObject) {
			let div = document.createElement("div");
			div.classList.add("searchHit");
			div.style.left = (bbox.x * scale) + "px";
			div.style.top = (bbox.y * scale) + "px";
			div.style.width = (bbox.w * scale) + "px";
			div.style.height = (bbox.h * scale) + "px";
			this.searchHitsNode.appendChild(div);
		}
	}

	_getLocalCoords(clientX, clientY) {
		const canvas = this.canvasNode;
		let x = clientX - canvas.getBoundingClientRect().left - canvas.clientLeft + canvas.scrollLeft;
		let y = clientY - canvas.getBoundingClientRect().top - canvas.clientTop + canvas.scrollTop;
		return { x, y };
	}
}

let zoomLevels = [ 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100, 110, 120, 130, 140, 150, 160, 170, 180, 190, 200 ];

// TODO - Split into separate file
class MupdfDocumentViewer {
	constructor(documentUri, initialPage, showDefaultUi) {

	}

	// TODO - Rename
	async initDocument(mupdfWorker, docName, editorMode) {
		// TODO validate worker param

		const pageCount = await mupdfWorker.countPages();
		const title = await mupdfWorker.documentTitle();

		// Use second page as default page size (the cover page is often differently sized)
		const defaultSize = await mupdfWorker.getPageSize(pageCount > 1 ? 2 : 1);

		this.mupdfWorker = mupdfWorker;
		this.pageCount = pageCount;
		this.title = title;
		this.defaultSize = defaultSize;
		this.searchNeedle = "";

		this.zoomLevel = 100;

		this.editorMode = editorMode;

		this.activePages = new Set();
		this.pageObserver = new IntersectionObserver(entries => {
			for (const entry of entries) {
				if (entry.isIntersecting) {
					this.activePages.add(entry.target);
				} else {
					this.activePages.delete(entry.target);
				}
			}
		}, {
			// This means we have roughly five viewports of vertical "head start" where
			// the page is rendered before it becomes visible
			// See
			rootMargin: "500% 0px",
		});

		// TODO
		// This is a hack to compensate for the lack of a priority queue
		// We wait until the user has stopped scrolling to load pages.
		let scrollTimer = null;
		document.addEventListener("scroll", (event) => {
			if (scrollTimer !== null)
				clearTimeout(scrollTimer);
			scrollTimer = setTimeout(() => {
				scrollTimer = null;
				this._updateView();
			}, 50);
		})

		// TODO - Add a second observer with bigger margin to recycle old pages

		//document.title = title ?? docName;
		//console.log("mupdf: Loaded", JSON.stringify(url), "with", pageCount, "pages.");

		//const rootDiv = document.createElement("div");

		this.gridMenubarDiv = document.getElementById("grid-menubar");
		this.gridSidebarDiv = document.getElementById("grid-sidebar");
		this.gridPagesDiv = document.getElementById("grid-pages");
		this.searchDialogDiv = document.getElementById("search-dialog");

		const pagesDiv = document.createElement("div");
		pagesDiv.id = "pages";
		pagesDiv.scrollTo(0, 0);

		let pages = new Array(pageCount);
		for (let i = 0; i < pageCount; ++i) {
			const page = new MupdfPageViewer(mupdfWorker, i, defaultSize, this._dpi());
			pages[i] = page;
			pagesDiv.appendChild(page.rootNode);
			this.pageObserver.observe(page.rootNode);
		}

		function isPage(element) {
			return element.tagName === "CANVAS" && element.closest("div.page") != null;
		}

		if (this.editorMode) {
			// TODO - use pointer events, pointercancel and setPointerCapture
			pagesDiv.onmousedown = (event) => {
				if (!isPage(event.target))
					return;

				const pageNumber = event.target.closest("div.page").pageNumber;
				pages[pageNumber].mouseDown(event, this._dpi());
				// TODO - remove "+ 1"
				this.currentSearchPage = pageNumber + 1;
			};
			pagesDiv.onmousemove = (event) => {
				if (!isPage(event.target))
					return;

				const pageNumber = event.target.closest("div.page").pageNumber;
				pages[pageNumber].mouseMove(event, this._dpi());
			};
			pagesDiv.onmouseup = (event) => {
				if (!isPage(event.target))
					return;

				const pageNumber = event.target.closest("div.page").pageNumber;
				pages[pageNumber].mouseUp(event, this._dpi());
			};
		}
		const searchDivInput = document.createElement("input");
		searchDivInput.id = "search-text";
		searchDivInput.type = "search";
		searchDivInput.size = 40;
		searchDivInput.addEventListener("input", () => {
			let newNeedle = searchDivInput.value ?? "";
			this.setSearch(newNeedle);
		});
		searchDivInput.addEventListener("keydown", (event) => {
			if (event.key == "Enter")
				this.runSearch(event.shiftKey ? -1 : 1);
		});
		const ltButton = document.createElement("button");
		ltButton.innerText = "<";
		ltButton.addEventListener("click", () => this.runSearch(-1));
		const gtButton = document.createElement("button");
		gtButton.innerText = ">";
		gtButton.addEventListener("click", () => this.runSearch(1));
		const hideButton = document.createElement("button");
		hideButton.innerText = "X";
		hideButton.addEventListener("click", () => this.hideSearchBox());
		const searchStatusDiv = document.createElement("div");
		searchStatusDiv.id = "search-status";
		searchStatusDiv.innerText = "-";

		const searchFlex = document.createElement("div");
		searchFlex.classList = [ "flex" ];
		searchFlex.append(searchDivInput, ltButton, gtButton, hideButton, searchStatusDiv);

		this.searchDialogDiv.appendChild(searchFlex);

		this.searchStatusDiv = searchStatusDiv;
		this.searchDivInput = searchDivInput;
		this.currentSearchPage = 1;

		// TODO use rootDiv instead
		pagesDiv.addEventListener("wheel", (event) => {
			if (event.ctrlKey || event.metaKey) {
				if (event.deltaY < 0)
					this.zoomIn();
				else if (event.deltaY > 0)
					this.zoomOut();
				event.preventDefault();
			}
		}, {passive: false});

		//this.rootDiv = rootDiv;
		this.pagesDiv = pagesDiv; // TODO - rename
		this.pages = pages;

		// TODO - remove await
		let outline = await mupdfWorker.documentOutline();
		let outlineNode = document.getElementById("outline");
		this.outlineNode = outlineNode;
		if (outline) {
			this._buildOutline(outlineNode, outline);
			this.showOutline();
		} else {
			this.hideOutline();
		}

		// TODO - remove once we add a priority queue
		for (let i = 0; i < Math.min(pageCount, 5); ++i) {
			this.activePages.add(pages[i].rootNode);
		}

		this._updateView();
	}

	// TODO - This destroys page elements - figure out correctness?
	// TODO - remove pagesDiv arg
	static showDocumentError(functionName, error, pagesDiv) {
		// TODO - this.clear() ?
		console.error(`mupdf.${functionName}: ${error.message}:\n${error.stack}`);
		let errorDiv = document.createElement("div");
		errorDiv.classList.add("error");
		errorDiv.textContent = error.name + ": " + error.message;
		pagesDiv.replaceChildren(errorDiv);
	}

	_updateView() {
		const dpi = this._dpi();
		for (const page of this.activePages) {
			this.pages[page.pageNumber].render(dpi, this.searchNeedle);
		}
	}

	// TODO - remove?
	_dpi() {
		return (this.zoomLevel * 96 / 100) | 0;
	}


	zoomIn() {
		// TODO - instead find next larger zoom
		let curr = zoomLevels.indexOf(this.zoomLevel);
		let next = zoomLevels[curr + 1];
		if (next)
			this.setZoom(next);
	}

	zoomOut() {
		let curr = zoomLevels.indexOf(this.zoomLevel);
		let next = zoomLevels[curr - 1];
		if (next)
			this.setZoom(next);
	}

	async setZoom(newZoom) {
		if (this.zoomLevel === newZoom)
			return;
		this.zoomLevel = newZoom;

		for (const page of this.pages) {
			page.setZoom(newZoom);
		}
		this._updateView();
	}


	clearSearch() {
		// TODO
	}

	setSearch(newNeedle) {
		this.searchStatusDiv.textContent = "";
		if (this.searchNeedle !== newNeedle) {
			this.searchNeedle = newNeedle;
			this._updateView();
		}
	}

	showSearchBox() {
		// TODO - Fix what happens when you re-open search with existing text
		this.searchDialogDiv.style.display = "flex";
		this.searchDivInput.focus();
		this.searchDivInput.select();
		this.setSearch(this.searchDivInput.value ?? "");
	}

	hideSearchBox() {
		this.searchStatusDiv.textContent = "";
		this.searchDialogDiv.style.display = "none";
		this.cancelSearch();
		this.setSearch("");
	}

	async runSearch(direction) {
		// TODO - internalize
		let searchStatus = document.getElementById("search-status");

		try {
			let page = this.currentSearchPage + direction;
			while (page >= 1 && page < this.pageCount) {
				// We run the check once per loop iteration,
				// in case the search was cancel during the 'await' below.
				if (this.searchNeedle === "") {
					searchStatus.textContent = "";
					return;
				}

				searchStatus.textContent = `Searching page ${page}.`;

				await this.pages[page]._loadPageSearch(this._dpi(), this.searchNeedle);
				const hits = this.pages[page].searchResultObject ?? [];
				if (hits.length > 0) {
					this.pages[page].rootNode.scrollIntoView();
					this.currentSearchPage = page;
					searchStatus.textContent = `${hits.length} hits on page ${page}.`;
					return;
				}

				page += direction;
			}

			searchStatus.textContent = "No more search hits.";
		}
		catch (error) {
			console.error(`mupdf.runSearch: ${error.message}:\n${error.stack}`);
		}
	}

	cancelSearch() {
		// TODO
	}

	showOutline() {
		this.gridSidebarDiv.style.display = "block";
		this.gridPagesDiv.classList.replace("sidebarHidden", "sidebarVisible");
	}

	hideOutline() {
		this.gridSidebarDiv.style.display = "none";
		this.gridPagesDiv.classList.replace("sidebarVisible", "sidebarHidden");
	}

	toggleOutline() {
		let node = this.gridSidebarDiv;
		if (node.style.display === "none" || node.style.display === "")
			this.showOutline();
		else
			this.hideOutline();
	}

	_buildOutline(listNode, outline) {
		for (let item of outline) {
			let itemNode = document.createElement("li");
			let aNode = document.createElement("a");
			// TODO - document the "+ 1" better
			aNode.href = `#page${item.page + 1}`;
			aNode.textContent = item.title;
			itemNode.appendChild(aNode);
			listNode.appendChild(itemNode);
			if (item.down) {
				itemNode = document.createElement("ul");
				this._buildOutline(itemNode, item.down);
				listNode.appendChild(itemNode);
			}
		}
	}

	clear() {
		this.pageObserver.disconnect();
		for (let page of this.pages ?? []) {
			page.clear();
		}
		this.pagesDiv.replaceChildren();
		this.outlineNode.replaceChildren();
		this.searchDialogDiv.replaceChildren();
		this.cancelSearch();
	}
}
