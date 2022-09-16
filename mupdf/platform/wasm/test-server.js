"use strict";

const http = require("http");
const express = require("express");

let app = express();

// Add some logging on each request
app.use(function(req, res, next) {
	const date = new Date().toISOString();
	console.log(
		`[${date}] "${cyan(req.method)} ${cyan(req.url)}" "${req.headers["user-agent"]}"`
	);
	next();
});

// Use Cross-Origin headers so browsers allow SharedArrayBuffer
app.use(function(req, res, next) {
	res.header("Cross-Origin-Opener-Policy", "same-origin");
	res.header("Cross-Origin-Embedder-Policy", "require-corp");
	next();
});


// Serve all static files in this folder
app.use(express.static(".", { fallthrough: false }));

// Add logging on failed requests.
app.use(function (error, req, res, next) {
	const date = new Date().toISOString();
	console.error(
		`[${date}] "${red(req.method)} ${red(req.url)}" Error (${red(error.status.toString())}): "${red(error.message)}"`
	);
	next(error);
});

let server = http.createServer(app);
server.listen(8000, "0.0.0.0");

function red(string) {
	return `\x1b[31m${string}\x1b[0m`;
}

function cyan(string) {
	return `\x1b[36m${string}\x1b[0m`;
}
