"use strict";

const http = require("http");
const express = require("express");

let app = express();
app.use(express.static("."));
let server = http.createServer(app);
server.listen(8000, "0.0.0.0");
