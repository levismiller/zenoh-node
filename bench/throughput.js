'use strict'

// Sustained pub/sub throughput benchmark.
//
// Subscribes to a key, fires N put()s, and measures how many messages/sec
// the native binding delivers to the subscriber through the local dispatch
// path. Run with a second process (or a peer on the network) publishing the
// same key to measure the full over-the-wire path.
//
//   node bench/throughput.js [messages] [payloadBytes]

const { Session } = require('..')

const N = Number(process.argv[2] || 100000)
const SIZE = Number(process.argv[3] || 32)
const KEY = 'bench/throughput'
const PAYLOAD = 'x'.repeat(SIZE)

const session = new Session()

let received = 0
let startT = 0n

console.log(`throughput: ${N} messages, ${SIZE} B each...`)

session.subscribe(KEY, () => {
  if (received === 0) startT = process.hrtime.bigint()
  received++
  if (received === N) finish()
})

// Give the subscription a moment to be declared before publishing.
setTimeout(() => {
  for (let k = 0; k < N; k++) session.put(KEY, PAYLOAD)
}, 500)

// Safety valve in case fewer than N messages arrive (e.g. no local delivery).
const guard = setTimeout(() => {
  if (received < N) {
    console.log(`  received only ${received}/${N} — try a two-process run`)
    finish()
  }
}, 15000)

function finish() {
  clearTimeout(guard)
  const seconds = Number(process.hrtime.bigint() - startT) / 1e9
  const rate = received / seconds
  const mib = (rate * SIZE) / (1024 * 1024)
  console.log(`  ${received} msgs in ${seconds.toFixed(3)} s`)
  console.log(`  ${Math.round(rate).toLocaleString()} msgs/sec  (~${mib.toFixed(1)} MiB/s)`)
  session.close()
  process.exit(0)
}
