const fs = require('fs');
const path = require('path');
const assert = require('assert');

console.log('=== RUNNING HARDWARE SIMULATOR & PIN INTEGRITY PROBE ===');

const htmlPath = path.join(__dirname, '../docs/interactive-wiring.html');
assert(fs.existsSync(htmlPath), 'docs/interactive-wiring.html must exist');
const html = fs.readFileSync(htmlPath, 'utf8');

// 1. Extract WIRE_NETS
const wireMatch = html.match(/const WIRE_NETS = (\[[\s\S]*?\]);/);
assert(wireMatch, 'WIRE_NETS array must be defined in HTML');
const wireNets = eval(wireMatch[1]);
console.log(`[PASS] Found ${wireNets.length} defined WIRE_NETS in interactive-wiring.html`);

// 2. Extract Pin Sockets from SVG
const pinRegex = /<circle[^>]*id="(pin-[^"]+)"[^>]*cx="([^"]+)"[^>]*cy="([^"]+)"[^>]*>/g;
const pins = new Map();
let match;
while ((match = pinRegex.exec(html)) !== null) {
  const [, id, cx, cy] = match;
  pins.set(id, { cx: parseFloat(cx), cy: parseFloat(cy) });
}
console.log(`[PASS] Extracted ${pins.size} pin sockets from SVG DOM`);

// 3. Verify Every Wire Endpoint
let brokenWires = 0;
wireNets.forEach((net, idx) => {
  const srcPin = pins.get(net.src);
  const dstPin = pins.get(net.dst);

  if (!srcPin) {
    console.error(`[FAIL] Wire #${idx} (${net.name}): src pin '${net.src}' NOT FOUND in SVG!`);
    brokenWires++;
  } else if (isNaN(srcPin.cx) || isNaN(srcPin.cy)) {
    console.error(`[FAIL] Wire #${idx} (${net.name}): src pin '${net.src}' has NaN coordinates!`);
    brokenWires++;
  }

  if (!dstPin) {
    console.error(`[FAIL] Wire #${idx} (${net.name}): dst pin '${net.dst}' NOT FOUND in SVG!`);
    brokenWires++;
  } else if (isNaN(dstPin.cx) || isNaN(dstPin.cy)) {
    console.error(`[FAIL] Wire #${idx} (${net.name}): dst pin '${net.dst}' has NaN coordinates!`);
    brokenWires++;
  }
});

assert.strictEqual(brokenWires, 0, `Encountered ${brokenWires} broken wire endpoints!`);
console.log(`[PASS] All ${wireNets.length} dynamic wire nets resolve to valid SVG pin sockets!`);

// 4. Verify Physical Hardware Alignment
assert(html.includes('W25Q64 BREAKOUT'), 'Flash module must be labeled W25Q64 BREAKOUT');
assert(html.includes('CH340 30-Pin Type-C'), 'ESP32 module must be labeled 30-Pin Type-C');
assert(html.includes('Blue Pill (Type-C)'), 'STM32 module must be labeled Type-C');
assert(html.includes('ILI9341 Red PCB (Non-Touch)'), 'TFT module must be labeled Red PCB (Non-Touch)');
assert(!html.includes('FLASH_WP_PULLUP'), 'Obsolete FLASH_WP_PULLUP wire net must be removed');
assert(!html.includes('FLASH_HOLD_PULLUP'), 'Obsolete FLASH_HOLD_PULLUP wire net must be removed');

// 5. Cross-Check src/TftTest.ino Pin Definitions
const inoPath = path.join(__dirname, '../src/TftTest.ino');
const ino = fs.readFileSync(inoPath, 'utf8');
assert(ino.includes('#define TFT_CS   15'), 'TftTest.ino must define TFT_CS 15');
assert(ino.includes('#define TFT_DC   2'), 'TftTest.ino must define TFT_DC 2');
assert(ino.includes('#define TFT_RST  4'), 'TftTest.ino must define TFT_RST 4');

// Verify Wiring_and_Pinouts.md Matches
const wiringDocPath = path.join(__dirname, '../docs/Wiring_and_Pinouts.md');
const wiringDoc = fs.readFileSync(wiringDocPath, 'utf8');
assert(wiringDoc.includes('GPIO 15'), 'Wiring doc must document GPIO 15');
assert(wiringDoc.includes('GPIO 2'), 'Wiring doc must document GPIO 2');
assert(wiringDoc.includes('GPIO 4'), 'Wiring doc must document GPIO 4');
assert(wiringDoc.includes('GPIO 16 (RX2)'), 'Wiring doc must document GPIO 16 (RX2)');
assert(wiringDoc.includes('GPIO 17 (TX2)'), 'Wiring doc must document GPIO 17 (TX2)');

console.log('=== ALL HARDWARE INTEGRITY & PROBE TESTS PASSED (100% OK) ===');
