#!/usr/bin/env node

let defaults =
    {input: 'input.br', output: 'output.txt', test_iters: 0, test_repeat: 100};

/* Parse command line arguments. */
let argv =
    require('yargs')
        .usage('Usage: $0 -i file -o file')
        .option(
            'input',
            {alias: 'i', default: defaults.input, describe: 'compressed file'})
        .option('output', {
          alias: 'o',
          default: defaults.output,
          describe: 'decompressed file'
        })
        .option('test_iters', {
          default: defaults.test_iters,
          describe: '# of times to run performance test'
        })
        .option('test_repeat', {
          default: defaults.test_repeat,
          describe: '# of times to decompress file in performance test'
        })
        .argv;

/* Read input. */
const fs = require('fs');
data = fs.readFileSync(argv.input);
if (!Buffer.isBuffer(data)) throw 'not a buffer';
const bytes = new Uint8Array(data);

/* Load and map brotli decoder module. */
global.window = {};
require('./decode.js')
const brotliDecode = window['BrotliDecode'];

/* Load "performance" module. */
const {PerformanceObserver, performance} = require('perf_hooks');

/* Performance test. */
for (let i = 0; i < argv.test_iters; ++i) {
  const a = performance.now();
  let result;
  for (let j = 0; j < argv.test_repeat; ++j) {
    result = brotliDecode(bytes);
  }
  const b = performance.now();
  const total_length = argv.test_repeat * result.length / (1024 * 1024);
  const total_time = (b - a) / 1000;

  console.log(
      total_length + 'MB / ' + total_time +
      's = ' + (total_length / total_time) + 'MB/s');
}

/* Decode and write output file. */
fs.writeFileSync(argv.output, new Buffer(brotliDecode(bytes)));
