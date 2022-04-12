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

"use strict";

// Thread to fetch data blocks while MuPDF worker thread is busy parsing.

let fetchStates = {};

function initFetch(id, url, contentLength, blockShift) {
	console.log("OPEN", url, "PROGRESSIVELY");
	fetchStates[id] = {
		url: url,
		blockShift: blockShift,
		blockSize: 1 << blockShift,
		contentLength: contentLength,
		map: new Array((contentLength >>> blockShift) + 1).fill(0),
		closed: false,
	};
}

async function fetchBlock(id, block) {
	let state = fetchStates[id];

	if (state.map[block] > 0)
		return;

	state.map[block] = 1;
	let contentLength = state.contentLength;
	let url = state.url;
	let start = block << state.blockShift;
	let end = start + state.blockSize;
	if (end > contentLength)
		end = contentLength;

	try {
		let response = await fetch(url, { headers: { Range: `bytes=${start}-${end-1}` } });
		if (state.closed)
			return;

		let buffer = await response.arrayBuffer();
		if (state.closed)
			return;

		console.log("READ", url, block+1, "/", state.map.length);
		state.map[block] = 2;
		postMessage(['DATA', id, block, buffer], [buffer]);

		prefetchNextBlock(id, block + 1);
	} catch(error) {
		state.map[block] = 0;
		postMessage(['ERROR', id, block, error.toString()]);
	}
}

function prefetchNextBlock(id, next_block) {
	let state = fetchStates[id];
	if (!state)
		return;

	// Don't prefetch if we're already waiting for any blocks.
	for (let block = 0; block < state.map.length; ++block)
		if (state.map[block] === 1)
			return;

	// Find next block to prefetch (starting with the last fetched block)
	for (let block = next_block; block < state.map.length; ++block)
		if (state.map[block] === 0)
			return fetchBlock(id, block);

	// Find next block to prefetch (starting from the beginning)
	for (let block = 0; block < state.map.length; ++block)
		if (state.map[block] === 0)
			return fetchBlock(id, block);

	console.log("ALL BLOCKS READ");
}

onmessage = function (event) {
	let [ cmd, id, arg ] = event.data;
	switch (cmd) {
	case 'OPEN':
		initFetch(id, arg[0], arg[1], arg[2]);
		break;
	case 'READ':
		fetchBlock(id, arg);
		break;
	case 'CLOSE':
		fetchStates[id].closed = true;
		delete fetchStates[id];
		break;
	}
}
