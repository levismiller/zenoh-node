'use strict'

// Round-trip request/reply latency benchmark.
//
// Declares a queryable that replies immediately, then times how long each
// get() → reply round trip takes through the native binding. Prints the
// median, mean, and p99 over N samples.
//
//   node bench/latency.js [samples] [payloadBytes]

const { Session } = require('..')

const N = Number(process.argv[2] || 10000)
const SIZE = Number(process.argv[3] || 32)
const KEY = 'bench/latency'
const PONG = 'x'.repeat(SIZE)

const session = new Session()
session.declareQueryable(KEY, (_keyexpr, _params, reply) => reply(PONG))

const samples = new Float64Array(N)
let i = 0

console.log(`latency: ${N} round trips, ${SIZE} B reply...`)

// Give peer discovery / declaration a moment to settle.
setTimeout(runNext, 500)

function runNext() {
  if (i >= N) return finish()
  const start = process.hrtime.bigint()
  session.get(
    KEY,
    '',
    () => {}, // onReply
    () => {
      const end = process.hrtime.bigint()
      samples[i++] = Number(end - start) / 1e6 // ms
      runNext()
    }
  )
}

function finish() {
  const arr = Array.from(samples.subarray(0, i)).sort((a, b) => a - b)
  const median = arr[Math.floor(arr.length * 0.5)]
  const p99 = arr[Math.floor(arr.length * 0.99)]
  const mean = arr.reduce((a, b) => a + b, 0) / arr.length
  console.log(`  median: ${median.toFixed(4)} ms`)
  console.log(`  mean:   ${mean.toFixed(4)} ms`)
  console.log(`  p99:    ${p99.toFixed(4)} ms`)
  session.close()
}
