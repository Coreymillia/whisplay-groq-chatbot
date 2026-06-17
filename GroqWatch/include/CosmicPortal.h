#pragma once

/*
 * CosmicPortal.h — Captive Art Portal for Core2Groq
 *
 * Extracted and adapted from COSMIC-CYD Captive Art Portal.
 * Runs as a WiFi AP + captive portal webserver with 70+ generative art modes.
 * Core2 display shows a minimal clock; all art renders in the visitor's browser.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

#include "GroqWatchLog.h"

// ── Portal configuration ──────────────────────────────────────────────────────
#define CP_AP_SSID   "GroqWatch Portal \xF0\x9F\x8E\xA8"
#define CP_DNS_PORT  53
#define CP_PORTAL_IP "192.168.4.1"

static WebServer  *cpServer = nullptr;
static DNSServer  *cpDnsServer = nullptr;
static bool        cpRunning = false;

// ── Message system ────────────────────────────────────────────────────────────
static String   cpPendingMsg = "";
static uint32_t cpMsgId = 0;
static String   cpVisitorMsg = "";
static uint32_t cpVisitorMsgAt = 0;
static bool     cpShowVisMsg = false;
static String   cpApName = "";  // Custom AP name (set via /setap)
static Preferences cpPrefs;     // NVS storage for portal settings  // Custom AP name (set via /setap)


// ═══════════════════════════════════════════════════════════════════════════
//  HTML Page Constants
// ═══════════════════════════════════════════════════════════════════════════

static const char INDEX_HTML[] = R"EOF(

<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>COSMIC-CYD</title><style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:radial-gradient(ellipse at 50% 50%,#0d003d,#000010 70%);
min-height:100vh;display:flex;flex-direction:column;align-items:center;
font-family:'Courier New',monospace;color:#fff;padding:20px 12px 28px}
.glw{position:fixed;border-radius:50%;filter:blur(90px);z-index:-1;pointer-events:none}
.g1{width:350px;height:350px;top:-120px;left:-120px;background:rgba(131,56,236,.3)}
.g2{width:300px;height:300px;bottom:-100px;right:-100px;background:rgba(6,255,208,.18)}
h1{font-size:clamp(1.1rem,5vw,1.7rem);letter-spacing:7px;text-align:center;margin-bottom:3px;
background:linear-gradient(90deg,#ff006e,#ff6b00,#ffd700,#06ffd0,#3a86ff,#8338ec,#ff006e);
-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text;
filter:drop-shadow(0 0 10px rgba(131,56,236,.7))}
.sub{font-size:.5rem;letter-spacing:8px;color:rgba(0,255,209,.45);margin-bottom:16px;text-align:center}
.cat{width:min(460px,92vw);font-size:.48rem;letter-spacing:5px;color:rgba(255,255,255,.3);
padding:10px 2px 4px;border-bottom:1px solid rgba(255,255,255,.06);margin-bottom:7px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(185px,1fr));
gap:7px;width:min(460px,92vw);margin-bottom:4px}
.card{display:block;padding:10px 13px;border-radius:10px;text-decoration:none;
background:rgba(10,0,40,.6);border:1px solid;transition:transform .15s,box-shadow .15s}
.card:hover,.card:active{transform:scale(1.03)}
.c1{border-color:rgba(131,56,236,.5);box-shadow:0 0 10px rgba(131,56,236,.08)}.c1:hover{box-shadow:0 0 22px rgba(131,56,236,.38)}
.c2{border-color:rgba(255,107,0,.45);box-shadow:0 0 10px rgba(255,107,0,.06)}.c2:hover{box-shadow:0 0 22px rgba(255,107,0,.32)}
.c3{border-color:rgba(6,255,208,.38);box-shadow:0 0 10px rgba(6,255,208,.06)}.c3:hover{box-shadow:0 0 22px rgba(6,255,208,.28)}
.c4{border-color:rgba(0,255,65,.45);box-shadow:0 0 10px rgba(0,255,65,.06)}.c4:hover{box-shadow:0 0 22px rgba(0,255,65,.35)}
.c5{border-color:rgba(255,0,170,.45);box-shadow:0 0 10px rgba(255,0,170,.06)}.c5:hover{box-shadow:0 0 22px rgba(255,0,170,.38)}
.c6{border-color:rgba(0,136,255,.45);box-shadow:0 0 10px rgba(0,136,255,.06)}.c6:hover{box-shadow:0 0 22px rgba(0,136,255,.38)}
.c7{border-color:rgba(180,180,255,.38);box-shadow:0 0 10px rgba(180,180,255,.06)}.c7:hover{box-shadow:0 0 22px rgba(180,180,255,.3)}
.c8{border-color:rgba(255,215,0,.38);box-shadow:0 0 10px rgba(255,215,0,.06)}.c8:hover{box-shadow:0 0 22px rgba(255,215,0,.3)}
.c9{border-color:rgba(255,80,0,.45);box-shadow:0 0 10px rgba(255,80,0,.06)}.c9:hover{box-shadow:0 0 22px rgba(255,80,0,.38)}
.cA{border-color:rgba(255,60,60,.42);box-shadow:0 0 10px rgba(255,60,60,.06)}.cA:hover{box-shadow:0 0 22px rgba(255,60,60,.35)}
.cB{border-color:rgba(0,210,255,.38);box-shadow:0 0 10px rgba(0,210,255,.06)}.cB:hover{box-shadow:0 0 22px rgba(0,210,255,.32)}
.cC{border-color:rgba(255,195,0,.42);box-shadow:0 0 10px rgba(255,195,0,.06)}.cC:hover{box-shadow:0 0 22px rgba(255,195,0,.32)}
.cD{border-color:rgba(160,255,80,.38);box-shadow:0 0 10px rgba(160,255,80,.06)}.cD:hover{box-shadow:0 0 22px rgba(160,255,80,.3)}
.cE{border-color:rgba(255,140,190,.38);box-shadow:0 0 10px rgba(255,140,190,.06)}.cE:hover{box-shadow:0 0 22px rgba(255,140,190,.3)}
.cF{border-color:rgba(90,190,255,.38);box-shadow:0 0 10px rgba(90,190,255,.06)}.cF:hover{box-shadow:0 0 22px rgba(90,190,255,.3)}
.icon{font-size:1.25rem;margin-bottom:3px;display:block}
.name{font-size:.7rem;letter-spacing:3px;font-weight:bold;display:block;margin-bottom:2px}
.desc{font-size:.4rem;letter-spacing:.7px;opacity:.46;display:block}
.n1{color:#c77dff}.n2{color:#ff9500}.n3{color:#06ffd0}.n4{color:#00ff41}.n5{color:#ff00aa}
.n6{color:#44aaff}.n7{color:#b4b4ff}.n8{color:#ffd700}.n9{color:#ff6622}
.nA{color:#ff5555}.nB{color:#00ddff}.nC{color:#ffcc00}.nD{color:#aaff66}.nE{color:#ffaacc}.nF{color:#88ccff}
footer{margin-top:16px;font-size:.4rem;letter-spacing:4px;color:rgba(255,255,255,.07);text-align:center}
</style></head><body>
<div class="glw g1"></div><div class="glw g2"></div>
<h1>COSMIC-CYD</h1><p class="sub">SELECT TRANSMISSION MODE</p>

<a href="/safety" style="display:block;width:min(460px,92vw);margin-bottom:14px;padding:11px 16px;border-radius:12px;text-decoration:none;background:rgba(40,0,0,.65);border:1px solid rgba(255,60,60,.55);box-shadow:0 0 18px rgba(255,40,40,.12);transition:box-shadow .15s" onmouseover="this.style.boxShadow='0 0 28px rgba(255,60,60,.4)'" onmouseout="this.style.boxShadow='0 0 18px rgba(255,40,40,.12)'">
<span style="display:flex;align-items:center;gap:10px">
<span style="font-size:1.3rem">&#x26A0;&#xFE0F;</span>
<span>
<span style="display:block;font-size:.7rem;letter-spacing:3px;font-weight:bold;color:#ff6b6b;margin-bottom:2px">FREE WIFI SAFETY &mdash; READ THIS</span>
<span style="display:block;font-size:.4rem;letter-spacing:.8px;color:rgba(255,150,150,.5)">WHAT EVIL PORTALS DON&apos;T WANT YOU TO KNOW &middot; TAP TO LEARN</span>
</span>
</span>
</a>

<div class="cat">&#x25C6; MATRIX RAIN</div>
<div class="grid">
  <a class="card c4" href="/matrix">
    <span class="icon">&#x2328;</span>
    <span class="name n4">MATRIX</span>
    <span class="desc">CLASSIC &middot; GREEN PHOSPHOR &middot; KANA</span>
  </a>
  <a class="card c5" href="/cyber">
    <span class="icon">&#x26A1;</span>
    <span class="name n5">CYBER RAIN</span>
    <span class="desc">NEON &middot; MULTICOLOR &middot; GLOW</span>
  </a>
  <a class="card c6" href="/binary">
    <span class="icon">&#x2B1B;</span>
    <span class="name n6">BINARY</span>
    <span class="desc">ZERO ONE &middot; ELECTRIC BLUE</span>
  </a>
  <a class="card c4" href="/mfire">
    <span class="icon">&#x1F525;</span>
    <span class="name n4">FIRE RAIN</span>
    <span class="desc">YELLOW &#x2192; ORANGE &#x2192; RED</span>
  </a>
  <a class="card cB" href="/mice">
    <span class="icon">&#x2744;</span>
    <span class="name nB">ICE RAIN</span>
    <span class="desc">WHITE &#x2192; CYAN &#x2192; BLUE</span>
  </a>
  <a class="card c7" href="/mstorm">
    <span class="icon">&#x26A1;</span>
    <span class="name n7">STORM RAIN</span>
    <span class="desc">LIGHTNING &middot; WHITE &amp; PURPLE</span>
  </a>
  <a class="card cA" href="/mblood">
    <span class="icon">&#x25CF;</span>
    <span class="name nA">BLOOD RAIN</span>
    <span class="desc">CRIMSON &middot; VISCOUS &middot; DARK</span>
  </a>
  <a class="card cC" href="/mgold">
    <span class="icon">&#x2B50;</span>
    <span class="name nC">GOLD RAIN</span>
    <span class="desc">AMBER &middot; GOLDEN &middot; WARM</span>
  </a>
  <a class="card c7" href="/mvoid">
    <span class="icon">&#x25A1;</span>
    <span class="name n7">VOID RAIN</span>
    <span class="desc">INVERTED &middot; DARK ON LIGHT</span>
  </a>
  <a class="card c3" href="/mphantom">
    <span class="icon">&#x25CA;</span>
    <span class="name n3">PHANTOM</span>
    <span class="desc">GHOSTLY &middot; PALE CYAN &middot; FADES</span>
  </a>
  <a class="card cB" href="/mripple">
    <span class="icon">&#x223F;</span>
    <span class="name nB">RIPPLE RAIN</span>
    <span class="desc">SINE WAVE &middot; AQUA &middot; FLOWS</span>
  </a>
  <a class="card c4" href="/mglitch">
    <span class="icon">&#x26A0;</span>
    <span class="name n4">GLITCH RAIN</span>
    <span class="desc">CORRUPT &middot; SPIKES &middot; NEON</span>
  </a>
</div>

<div class="cat">&#x25C6; FRACTALS &amp; MATHEMATICS</div>
<div class="grid">
  <a class="card c3" href="/fractal">
    <span class="icon">&#x2726;</span>
    <span class="name n3">JULIA SET</span>
    <span class="desc">ESCAPE TIME &middot; C3 RENDERED</span>
  </a>
  <a class="card cE" href="/hopalong">
    <span class="icon">&#x2299;</span>
    <span class="name nE">HOPALONG</span>
    <span class="desc">ATTRACTOR &middot; CHAOS ORBIT</span>
  </a>
  <a class="card c1" href="/interference">
    <span class="icon">&#x223F;</span>
    <span class="name n1">INTERFERENCE</span>
    <span class="desc">WAVE PATTERNS &middot; DUAL SOURCE</span>
  </a>
  <a class="card cC" href="/voronoi">
    <span class="icon">&#x2B21;</span>
    <span class="name nC">VORONOI</span>
    <span class="desc">CELLS &middot; MOVING SEEDS</span>
  </a>
  <a class="card c3" href="/strange">
    <span class="icon">&#x221E;</span>
    <span class="name n3">STRANGE</span>
    <span class="desc">CLIFFORD ATTRACTOR &middot; CHAOS</span>
  </a>
  <a class="card c2" href="/lissajous">
    <span class="icon">&#x224B;</span>
    <span class="name n2">LISSAJOUS</span>
    <span class="desc">HARMONIC FIGURES &middot; DRIFT</span>
  </a>
  <a class="card cC" href="/sierpinski">
    <span class="icon">&#x25B2;</span>
    <span class="name nC">SIERPINSKI</span>
    <span class="desc">CHAOS GAME &middot; TRIANGLE</span>
  </a>
  <a class="card c1" href="/spirograph">
    <span class="icon">&#x25CB;</span>
    <span class="name n1">SPIROGRAPH</span>
    <span class="desc">HYPOTROCHOID &middot; GEARS</span>
  </a>
  <a class="card cD" href="/barnsley">
    <span class="icon">&#x2AEB;</span>
    <span class="name nD">BARNSLEY FERN</span>
    <span class="desc">IFS FRACTAL &middot; NATURE MATH</span>
  </a>
</div>

<div class="cat">&#x25C6; ABSTRACT</div>
<div class="grid">
  <a class="card c1" href="/mandala">
    <span class="icon">&#x2B21;</span>
    <span class="name n1">MANDALA</span>
    <span class="desc">SACRED GEOMETRY &middot; RINGS</span>
  </a>
  <a class="card c2" href="/plasma">
    <span class="icon">&#x25C9;</span>
    <span class="name n2">PLASMA</span>
    <span class="desc">LAVA BLOBS &middot; CSS BLEND</span>
  </a>
  <a class="card c7" href="/starfield">
    <span class="icon">&#x2605;</span>
    <span class="name n7">STARFIELD</span>
    <span class="desc">WARP SPEED &middot; 3D PROJECTION</span>
  </a>
  <a class="card c8" href="/particles">
    <span class="icon">&#x22C6;</span>
    <span class="name n8">PARTICLES</span>
    <span class="desc">CONSTELLATION &middot; MESH</span>
  </a>
  <a class="card c9" href="/tunnel">
    <span class="icon">&#x25CE;</span>
    <span class="name n9">TUNNEL</span>
    <span class="desc">VORTEX &middot; ROTATING RINGS</span>
  </a>
  <a class="card cE" href="/kaleidoscope">
    <span class="icon">&#x1F52E;</span>
    <span class="name nE">KALEIDOSCOPE</span>
    <span class="desc">MIRROR &middot; 8-FOLD SYMMETRY</span>
  </a>
  <a class="card cF" href="/noise">
    <span class="icon">&#x25A6;</span>
    <span class="name nF">NOISE FIELD</span>
    <span class="desc">SINE NOISE &middot; PSYCHEDELIC</span>
  </a>
  <a class="card c9" href="/lava2">
    <span class="icon">&#x25CF;</span>
    <span class="name n9">LAVA LAMP</span>
    <span class="desc">METABALLS &middot; FLUID BLOBS</span>
  </a>
</div>

<div class="cat">&#x25C6; SIMULATIONS</div>
<div class="grid">
  <a class="card c2" href="/campfire">
    <span class="icon">&#x1F525;</span>
    <span class="name n2">CAMPFIRE</span>
    <span class="desc">FLAME TONGUES &middot; WARM GLOW</span>
  </a>
  <a class="card cF" href="/raindrops">
    <span class="icon">&#x1F4A7;</span>
    <span class="name nF">RAINDROPS</span>
    <span class="desc">WATER RIPPLES &middot; EXPANDING</span>
  </a>
  <a class="card c3" href="/gameoflife">
    <span class="icon">&#x25A3;</span>
    <span class="name n3">GAME OF LIFE</span>
    <span class="desc">CONWAY &middot; CELLULAR AUTOMATON</span>
  </a>
  <a class="card cD" href="/aurora">
    <span class="icon">&#x1F30C;</span>
    <span class="name nD">AURORA</span>
    <span class="desc">BOREALIS &middot; SINE WAVES &middot; GLOW</span>
  </a>
  <a class="card cC" href="/dragon">
    <span class="icon">&#x1F409;</span>
    <span class="name nC">DRAGON CURVE</span>
    <span class="desc">L-SYSTEM &middot; ANIMATED BUILD</span>
  </a>
</div>

<div class="cat">&#x25C6; MATHEMATICS DEEP</div>
<div class="grid">
  <a class="card c5" href="/apollonian">
    <span class="icon">&#x25CB;</span>
    <span class="name n5">APOLLONIAN</span>
    <span class="desc">CIRCLE PACKING &middot; DESCARTES</span>
  </a>
  <a class="card c2" href="/sunflower">
    <span class="icon">&#x1F33B;</span>
    <span class="name n2">SUNFLOWER</span>
    <span class="desc">PHYLLOTAXIS &middot; GOLDEN RATIO</span>
  </a>
  <a class="card cA" href="/quasicrystal">
    <span class="icon">&#x2736;</span>
    <span class="name nA">QUASICRYSTAL</span>
    <span class="desc">ROTATED WAVES &middot; INTERFERENCE</span>
  </a>
  <a class="card cC" href="/lorenz">
    <span class="icon">&#x1F98B;</span>
    <span class="name nC">LORENZ</span>
    <span class="desc">BUTTERFLY ATTRACTOR &middot; CHAOS</span>
  </a>
  <a class="card c9" href="/mandelbrot">
    <span class="icon">&#x2665;</span>
    <span class="name n9">MANDELBROT</span>
    <span class="desc">COMPLEX PLANE &middot; COLOR CYCLE</span>
  </a>
</div>

<div class="cat">&#x25C6; 3D WORLDS</div>
<div class="grid">
  <a class="card cF" href="/cube3d">
    <span class="icon">&#x2B1C;</span>
    <span class="name nF">CUBE 3D</span>
    <span class="desc">WIREFRAME &middot; CUBE + ICOSAHEDRON</span>
  </a>
  <a class="card c7" href="/torus">
    <span class="icon">&#x25EF;</span>
    <span class="name n7">TORUS</span>
    <span class="desc">3D DONUT &middot; PERSPECTIVE WIRE</span>
  </a>
  <a class="card c3" href="/hypercube">
    <span class="icon">&#x2B1B;</span>
    <span class="name n3">HYPERCUBE</span>
    <span class="desc">4D TESSERACT &middot; DUAL ROTATION</span>
  </a>
</div>

<div class="cat">&#x25C6; GENERATIVE LIFE</div>
<div class="grid">
  <a class="card c1" href="/reaction">
    <span class="icon">&#x1F9EA;</span>
    <span class="name n1">REACTION</span>
    <span class="desc">GRAY-SCOTT &middot; TURING PATTERNS</span>
  </a>
  <a class="card c8" href="/maze">
    <span class="icon">&#x2796;</span>
    <span class="name n8">MAZE</span>
    <span class="desc">DFS CARVE &middot; ANIMATED BUILD</span>
  </a>
  <a class="card c6" href="/vines">
    <span class="icon">&#x1F343;</span>
    <span class="name n6">VINES</span>
    <span class="desc">BRANCHING GROWTH &middot; LEAVES</span>
  </a>
  <a class="card cD" href="/snowflakes">
    <span class="icon">&#x2744;</span>
    <span class="name nD">SNOWFLAKES</span>
    <span class="desc">PROCEDURAL CRYSTAL &middot; FALLING</span>
  </a>
  <a class="card cB" href="/cityflow">
    <span class="icon">&#x1F3D9;</span>
    <span class="name nB">CITY FLOW</span>
    <span class="desc">GRID CITY &middot; TRAFFIC LIGHTS</span>
  </a>
</div>

<div class="cat">&#x25C6; REFLECTIONS</div>
<div class="grid">
  <a class="card cE" href="/retrogeo">
    <span class="icon">&#x25B2;</span>
    <span class="name nE">RETRO GEO</span>
    <span class="desc">80s SHAPES &middot; NEON SCAN LINES</span>
  </a>
  <a class="card c4" href="/mirrorblob">
    <span class="icon">&#x1F300;</span>
    <span class="name n4">MIRROR BLOB</span>
    <span class="desc">4-WAY SYMMETRY &middot; ORGANIC</span>
  </a>
</div>

<div class="cat">&#x25C6; GAMES</div>
<div class="grid">
  <a class="card c1" href="/snake">
    <span class="icon">&#x1F40D;</span>
    <span class="name n1">SNAKE</span>
    <span class="desc">CLASSIC &middot; NEON &middot; TOUCH + KEYS</span>
  </a>
  <a class="card cB" href="/breakout">
    <span class="icon">&#x1F9F1;</span>
    <span class="name nB">BREAKOUT</span>
    <span class="desc">BRICKS &middot; CYAN GLOW &middot; TOUCH</span>
  </a>
  <a class="card c7" href="/tetris">
    <span class="icon">&#x25A6;</span>
    <span class="name n7">TETRIS</span>
    <span class="desc">TETROMINOES &middot; GHOST &middot; LEVELS</span>
  </a>
  <a class="card c9" href="/dodge">
    <span class="icon">&#x1F680;</span>
    <span class="name n9">STELLAR DODGE</span>
    <span class="desc">ASTEROIDS &middot; COMETS &middot; SURVIVE</span>
  </a>
  <a class="card cF" href="/asteroids">
    <span class="icon">&#x2604;</span>
    <span class="name nF">ASTEROIDS</span>
    <span class="desc">SHOOT &middot; THRUST &middot; WARP</span>
  </a>
</div>

<div class="cat">&#x25C6; SPACE &amp; COSMOS</div>
<div class="grid">
  <a class="card cD" href="/deepstars">
    <span class="icon">&#x2B50;</span>
    <span class="name nD">DEEP STARS</span>
    <span class="desc">3D PARALLAX &middot; NEBULA &middot; WARP</span>
  </a>
  <a class="card c9" href="/nebula">
    <span class="icon">&#x1F300;</span>
    <span class="name n9">NEBULA</span>
    <span class="desc">GAS CLOUD &middot; STAR FIELD</span>
  </a>
  <a class="card cA" href="/plasmaglobe">
    <span class="icon">&#x26A1;</span>
    <span class="name nA">PLASMA GLOBE</span>
    <span class="desc">ELECTRIC TENDRILS &middot; GLOW</span>
  </a>
  <a class="card cF" href="/warpgrid">
    <span class="icon">&#x2395;</span>
    <span class="name nF">WARP GRID</span>
    <span class="desc">3D MESH &middot; WAVE DISTORT</span>
  </a>
</div>

<div class="cat">&#x25C6; PARTICLE SYSTEMS</div>
<div class="grid">
  <a class="card c2" href="/fireworks">
    <span class="icon">&#x1F386;</span>
    <span class="name n2">FIREWORKS</span>
    <span class="desc">LAUNCH &middot; BURST &middot; GRAVITY</span>
  </a>
  <a class="card cE" href="/bounceballs">
    <span class="icon">&#x25CF;</span>
    <span class="name nE">BOUNCE BALLS</span>
    <span class="desc">NEON GLOW &middot; PHYSICS</span>
  </a>
  <a class="card c5" href="/flowfield">
    <span class="icon">&#x27BF;</span>
    <span class="name n5">FLOW FIELD</span>
    <span class="desc">PERLIN FLOW &middot; PARTICLES</span>
  </a>
  <a class="card c1" href="/neonrain">
    <span class="icon">&#x2605;</span>
    <span class="name n1">NEON RAIN</span>
    <span class="desc">SYMBOL SHOWER &middot; GREEN &middot; BLUE</span>
  </a>
</div>

<div class="cat">&#x25C6; LIFE &amp; PHYSICS</div>
<div class="grid">
  <a class="card c3" href="/coral">
    <span class="icon">&#x1F420;</span>
    <span class="name n3">CORAL REEF</span>
    <span class="desc">CELLULAR GROWTH &middot; OCEAN</span>
  </a>
  <a class="card cB" href="/sandfall">
    <span class="icon">&#x23F3;</span>
    <span class="name nB">SAND FALL</span>
    <span class="desc">CELLULAR AUTOMATON &middot; PHYSICS</span>
  </a>
  <a class="card c8" href="/lightning">
    <span class="icon">&#x26A1;</span>
    <span class="name n8">LIGHTNING</span>
    <span class="desc">FRACTAL BRANCH &middot; ELECTRIC</span>
  </a>
  <a class="card c6" href="/crystal">
    <span class="icon">&#x1F48E;</span>
    <span class="name n6">CRYSTAL</span>
    <span class="desc">HEXAGONAL &middot; PRISMATIC GROW</span>
  </a>
</div>

<div class="cat">&#x25C6; PSYCHEDELIC GEOMETRY</div>
<div class="grid">
  <a class="card c7" href="/acidspiral">
    <span class="icon">&#x1F300;</span>
    <span class="name n7">ACID SPIRAL</span>
    <span class="desc">MULTI-ARM &middot; HSL CYCLING</span>
  </a>
  <a class="card cC" href="/goop">
    <span class="icon">&#x1FAA0;</span>
    <span class="name nC">GOOP</span>
    <span class="desc">ORGANIC BLOBS &middot; TENTACLES</span>
  </a>
  <a class="card c4" href="/metaballs">
    <span class="icon">&#x25D5;</span>
    <span class="name n4">METABALLS</span>
    <span class="desc">ISO-SURFACE &middot; MORPHING</span>
  </a>
  <a class="card c0" href="/wormhole">
    <span class="icon">&#x1F30C;</span>
    <span class="name n0">WORMHOLE</span>
    <span class="desc">RING VORTEX &middot; CONVERGING</span>
  </a>
  <a class="card cD" href="/cwaves">
    <span class="icon">&#x223F;</span>
    <span class="name nD">C-WAVES</span>
    <span class="desc">C-SHAPED SINE &middot; INTERFERENCE</span>
  </a>
  <a class="card cF" href="/dna">
    <span class="icon">&#x1F9EC;</span>
    <span class="name nF">DNA HELIX</span>
    <span class="desc">DOUBLE HELIX &middot; 3D ROTATE</span>
  </a>
</div>

<footer>esp32 cyd &middot; wifi ap &middot; 192.168.4.1 &middot; 60+ modes &middot; sd gallery</footer>
<div style="width:min(460px,92vw);margin:14px auto 0;background:rgba(10,0,40,.7);border:1px solid rgba(131,56,236,.4);border-radius:12px;padding:14px 16px">
<p style="font-size:.5rem;letter-spacing:4px;color:rgba(131,56,236,.8);margin-bottom:8px;text-align:center">&#x2709; SEND A MESSAGE TO THE GALLERY</p>
<div style="display:flex;gap:8px">
<input id="vmsg" maxlength="50" placeholder="Say hi to the operator..."
  style="flex:1;background:rgba(0,0,0,.5);border:1px solid rgba(131,56,236,.5);border-radius:8px;color:#fff;padding:8px 10px;font-family:monospace;font-size:.65rem;letter-spacing:1px;outline:none">
<button onclick="sendVMsg()"
  style="background:rgba(131,56,236,.3);border:1px solid rgba(131,56,236,.6);color:#c77dff;border-radius:8px;padding:8px 14px;font-family:monospace;font-size:.6rem;letter-spacing:2px;cursor:pointer">SEND</button>
</div>
<p id="vmsg-status" style="font-size:.42rem;letter-spacing:2px;color:rgba(6,255,208,0);text-align:center;margin-top:6px;transition:color .3s">&#x2713; MESSAGE SENT</p>
</div>
<script>var _mi=0;
setInterval(function(){
  fetch('/api/msg').then(function(r){return r.json();}).then(function(d){
    if(d.id>_mi){_mi=d.id;showMsg(d.msg);}
  }).catch(function(){});
},2500);
function showMsg(m){
  var t=document.createElement('div');
  t.style.cssText='position:fixed;bottom:28px;left:50%;transform:translateX(-50%);'
    +'background:rgba(13,0,61,.93);border:1px solid rgba(131,56,236,.65);'
    +'color:#c77dff;padding:12px 22px;border-radius:12px;font-family:monospace;'
    +'font-size:.72rem;letter-spacing:2px;z-index:9999;text-align:center;'
    +'max-width:300px;box-shadow:0 0 22px rgba(131,56,236,.38)';
  t.textContent=m;
  document.body.appendChild(t);
  setTimeout(function(){t.remove();},5000);
}
function sendVMsg(){
  var inp=document.getElementById('vmsg');
  var m=inp.value.trim();
  if(!m)return;
  fetch('/api/visitor-msg',{method:'POST',headers:{'Content-Type':'text/plain'},body:m})
    .then(function(){
      inp.value='';
      var s=document.getElementById('vmsg-status');
      s.style.color='rgba(6,255,208,.9)';
      setTimeout(function(){s.style.color='rgba(6,255,208,0)';},3000);
    }).catch(function(){});
}
</script>
</body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char SAFETY_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>FREE WIFI SAFETY &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:radial-gradient(ellipse at 50% 50%,#1a0000,#000010 70%);
min-height:100vh;display:flex;flex-direction:column;align-items:center;
font-family:'Courier New',monospace;color:#fff;padding:20px 12px 36px}
.glw{position:fixed;border-radius:50%;filter:blur(90px);z-index:-1;pointer-events:none}
.g1{width:350px;height:350px;top:-120px;left:-80px;background:rgba(200,20,20,.18)}
.g2{width:280px;height:280px;bottom:-80px;right:-80px;background:rgba(131,56,236,.15)}
.nav{width:min(480px,94vw);display:flex;justify-content:space-between;align-items:center;margin-bottom:22px}
.nav a{color:rgba(255,100,100,.7);text-decoration:none;font-size:.55rem;letter-spacing:3px}
.nav a:hover{color:#ff6b6b}
.nav span{color:rgba(255,80,80,.3);font-size:.48rem;letter-spacing:3px}
h1{font-size:clamp(1rem,5vw,1.5rem);letter-spacing:5px;text-align:center;margin-bottom:4px;color:#ff6b6b;
filter:drop-shadow(0 0 12px rgba(255,60,60,.6))}
.sub{font-size:.48rem;letter-spacing:7px;color:rgba(255,100,100,.4);margin-bottom:22px;text-align:center}
.w{width:min(480px,94vw)}
.card{background:rgba(15,0,0,.7);border-radius:12px;padding:16px 18px;margin-bottom:12px;border:1px solid}
.red{border-color:rgba(255,60,60,.45);box-shadow:0 0 14px rgba(255,40,40,.08)}
.grn{border-color:rgba(0,220,120,.38);box-shadow:0 0 14px rgba(0,200,100,.08)}
.prp{border-color:rgba(131,56,236,.38);box-shadow:0 0 14px rgba(131,56,236,.08)}
.amb{border-color:rgba(255,180,0,.38);box-shadow:0 0 14px rgba(255,160,0,.08)}
.card h2{font-size:.68rem;letter-spacing:4px;margin-bottom:10px;display:flex;align-items:center;gap:8px}
.red h2{color:#ff6b6b}.grn h2{color:#06ffd0}.prp h2{color:#c77dff}.amb h2{color:#ffd700}
.card p,.card li{font-size:.55rem;letter-spacing:.8px;line-height:1.7;color:rgba(255,255,255,.65)}
.card ul{padding-left:14px;margin-top:6px}
.card li{margin-bottom:4px}
.tag{display:inline-block;padding:2px 8px;border-radius:4px;font-size:.42rem;letter-spacing:2px;margin-right:4px;margin-top:4px;font-weight:bold}
.tred{background:rgba(255,60,60,.2);color:#ff8888;border:1px solid rgba(255,60,60,.4)}
.tgrn{background:rgba(0,220,120,.15);color:#06ffd0;border:1px solid rgba(0,200,100,.35)}
.rule{font-size:.62rem;letter-spacing:2px;color:#fff;background:rgba(131,56,236,.2);
border-radius:8px;padding:10px 14px;margin-bottom:8px;border-left:3px solid rgba(131,56,236,.7);line-height:1.6}
.rule span{color:#c77dff;font-weight:bold}
.ok{background:rgba(0,30,20,.8);border:1px solid rgba(0,220,120,.4);border-radius:12px;padding:16px 18px;margin-bottom:12px}
.ok h2{font-size:.68rem;letter-spacing:4px;color:#06ffd0;margin-bottom:10px}
.ok p{font-size:.53rem;letter-spacing:.8px;line-height:1.7;color:rgba(255,255,255,.65)}
.ok .tick{color:#06ffd0;margin-right:6px}
footer{margin-top:18px;font-size:.4rem;letter-spacing:4px;color:rgba(255,255,255,.07);text-align:center}
</style></head><body>
<div class="glw g1"></div><div class="glw g2"></div>
<div class="w">
<div class="nav"><a href="/">&#x2B21; BACK TO PORTAL</a><span>SAFETY README</span></div>
<h1>&#x26A0; FREE WIFI SAFETY</h1>
<p class="sub">WHAT EVERY PORTAL CLICKER SHOULD KNOW</p>

<div class="card red">
<h2>&#x1F3AF; WHAT IS A CAPTIVE PORTAL?</h2>
<p>When you connect to a WiFi network and get redirected to a webpage before you can access the internet &mdash; that is a captive portal. Hotels, airports, coffee shops, and yes, this little cosmic device all use them. Most are harmless. Some are not.</p>
</div>

<div class="card red">
<h2>&#x1F480; HOW EVIL PORTALS ATTACK YOU</h2>
<p>A bad actor sets up a hotspot with a name like <b style="color:#ff8888">"FREE Airport WiFi"</b> or <b style="color:#ff8888">"Starbucks Guest"</b> &mdash; indistinguishable from the real thing. When you connect, the portal asks you to &ldquo;log in&rdquo; to get online. That login form is a trap. Watch out for:</p>
<ul>
<li>Portals asking for your <b style="color:#ff8888">email &amp; password</b> &mdash; especially Gmail, Facebook, Apple ID</li>
<li>Portals that mimic <b style="color:#ff8888">real company login pages</b> (Google, Microsoft, Instagram)</li>
<li>Portals asking for a <b style="color:#ff8888">credit card</b> to &ldquo;verify your identity&rdquo;</li>
<li>Portals that install an <b style="color:#ff8888">app or profile</b> &ldquo;required to connect&rdquo;</li>
<li>Any portal that feels <b style="color:#ff8888">slightly off</b> &mdash; trust that instinct</li>
</ul>
<div style="margin-top:10px">
<span class="tag tred">CREDENTIAL THEFT</span>
<span class="tag tred">PHISHING</span>
<span class="tag tred">MAN IN THE MIDDLE</span>
<span class="tag tred">SESSION HIJACK</span>
</div>
</div>

<div class="card amb">
<h2>&#x1F9E0; THE GOLDEN RULES</h2>
<div class="rule"><span>RULE 1 &mdash;</span> Never enter your real password on a portal login page. Ever. Legitimate public WiFi does not need your Google or social media password.</div>
<div class="rule"><span>RULE 2 &mdash;</span> Check the URL bar. Portals are always HTTP, never HTTPS. If a page is asking for sensitive info and it&apos;s not HTTPS &mdash; close it immediately.</div>
<div class="rule"><span>RULE 3 &mdash;</span> Verify the network name with staff before connecting in public places. Evil portals often have very similar names to the real network.</div>
<div class="rule"><span>RULE 4 &mdash;</span> Use a VPN on any public WiFi. Even a legitimate network can have bad actors watching traffic.</div>
<div class="rule"><span>RULE 5 &mdash;</span> If a portal asks you to install anything &mdash; disconnect immediately and report it.</div>
</div>

<div class="ok">
<h2>&#x2728; ABOUT THIS PORTAL</h2>
<p><span class="tick">&#x2713;</span>This is COSMIC-S3 &mdash; a WiFi art gallery running on a tiny ESP32-S3 chip.<br>
<span class="tick">&#x2713;</span>It does not have internet access. Nothing you do here leaves this device.<br>
<span class="tick">&#x2713;</span>It collects zero personal data. No emails, no passwords, no accounts.<br>
<span class="tick">&#x2713;</span>It never asks for credentials of any kind.<br>
<span class="tick">&#x2713;</span>It is purely a canvas &mdash; 60+ generative art &amp; animation modes for you to explore.<br>
<span class="tick">&#x2713;</span>You found something weird and wonderful. That&apos;s it. Enjoy it.</p>
</div>

<div class="card prp">
<h2>&#x1F4F1; IF YOU&apos;RE EVER UNSURE</h2>
<p>The safest move is always to disconnect and forget the network. You can reconnect to your mobile data in seconds. No free WiFi is worth your account credentials or personal information. Stay cosmic, stay safe. &#x1F320;</p>
</div>

</div>
<footer>cosmic-s3 &middot; esp32-s3 t-qt pro &middot; no internet &middot; no tracking &middot; just art</footer>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char MANDALA_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MANDALA · COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:radial-gradient(ellipse at 35% 45%,#0d003d 0%,#000010 60%,#001a20 100%);
min-height:100vh;display:flex;flex-direction:column;align-items:center;
justify-content:center;font-family:'Courier New',monospace;color:#fff;
overflow-x:hidden;padding:56px 16px 24px}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,10,.75);backdrop-filter:blur(8px);
border-bottom:1px solid rgba(131,56,236,.3);z-index:99;display:flex;align-items:center;justify-content:space-between}
.nav a{color:#8338ec;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#c77dff}
.nav span{color:rgba(255,255,255,.18);font-size:.5rem;letter-spacing:3px}
.glw{position:fixed;border-radius:50%;filter:blur(80px);z-index:-1;pointer-events:none}
.g1{width:320px;height:320px;top:-100px;left:-100px;background:rgba(123,47,255,.35)}
.g2{width:260px;height:260px;top:-80px;right:-80px;background:rgba(0,255,209,.2)}
.g3{width:300px;height:300px;bottom:-100px;left:-80px;background:rgba(58,134,255,.28)}
.g4{width:240px;height:240px;bottom:-80px;right:-80px;background:rgba(255,0,110,.22)}
.stars{position:fixed;inset:0;z-index:-1}
.s{position:absolute;background:#fff;border-radius:50%}
h1{font-size:clamp(1.5rem,5vw,2.2rem);letter-spacing:6px;text-align:center;
background:linear-gradient(90deg,#ff006e,#ff6b00,#ffd700,#06ffd0,#3a86ff,#8338ec,#ff006e);
-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text;
filter:drop-shadow(0 0 8px rgba(131,56,236,.8));margin-bottom:4px}
.sub{font-size:.65rem;letter-spacing:9px;color:rgba(0,255,209,.65);text-align:center;margin-bottom:20px}
.mandala{position:relative;width:min(260px,72vw);height:min(260px,72vw);margin:0 auto 22px}
.r,.d{position:absolute;border-radius:50%;top:50%;left:50%;transform:translate(-50%,-50%)}
.r0{width:100%;height:100%;background:conic-gradient(#ff006e,#ff6b00,#ffd700,#06ffd0,#3a86ff,#8338ec,#ff006e)}
.d0{width:91%;height:91%;background:#000010}
.r1{width:84%;height:84%;background:conic-gradient(#3a86ff,#8338ec,#ff006e,#ffd700,#06ffd0,#ff6b00,#3a86ff);transform:translate(-50%,-50%) rotate(26deg)}
.d1{width:76%;height:76%;background:#000010}
.r2{width:70%;height:70%;background:conic-gradient(#ffd700,#06ffd0,#ff006e,#8338ec,#ff6b00,#3a86ff,#ffd700);transform:translate(-50%,-50%) rotate(52deg)}
.d2{width:62%;height:62%;background:#000010}
.r3{width:56%;height:56%;background:conic-gradient(#06ffd0,#ff6b00,#8338ec,#ff006e,#3a86ff,#ffd700,#06ffd0);transform:translate(-50%,-50%) rotate(78deg)}
.d3{width:48%;height:48%;background:#000010}
.r4{width:43%;height:43%;background:conic-gradient(#8338ec,#3a86ff,#06ffd0,#ff6b00,#ff006e,#ffd700,#8338ec);transform:translate(-50%,-50%) rotate(104deg)}
.d4{width:35%;height:35%;background:#000010}
.r5{width:30%;height:30%;background:conic-gradient(#ff6b00,#ffd700,#3a86ff,#06ffd0,#8338ec,#ff006e,#ff6b00);transform:translate(-50%,-50%) rotate(130deg)}
.sp{position:absolute;width:1px;height:100%;
background:linear-gradient(transparent 0%,rgba(255,255,255,.2) 30%,rgba(255,255,255,.45) 50%,rgba(255,255,255,.2) 70%,transparent 100%);
top:0;left:calc(50% - .5px);transform-origin:center}
.hl{position:absolute;border-radius:50%;top:50%;left:50%;transform:translate(-50%,-50%);pointer-events:none;border:1px solid}
.h1{width:110%;height:110%;border-color:rgba(131,56,236,.5);box-shadow:0 0 15px rgba(131,56,236,.35),inset 0 0 15px rgba(131,56,236,.12)}
.h2{width:118%;height:118%;border-color:rgba(0,255,209,.25);box-shadow:0 0 10px rgba(0,255,209,.2)}
.core{width:17%;height:17%;background:radial-gradient(circle,#fff 0%,#e040fb 45%,rgba(131,56,236,0) 100%);box-shadow:0 0 20px #c77dff,0 0 40px rgba(131,56,236,.5)}
.card{background:rgba(10,0,45,.55);border:1px solid rgba(131,56,236,.4);border-radius:12px;padding:16px 26px;text-align:center;width:min(290px,88vw);box-shadow:0 0 24px rgba(131,56,236,.15),inset 0 1px 0 rgba(255,255,255,.04)}
.lbl{font-size:.58rem;letter-spacing:3px;color:rgba(0,255,209,.55);margin-bottom:3px;text-transform:uppercase}
.val{font-size:.95rem;font-weight:bold;margin-bottom:13px}
.pink{color:#ff006e;text-shadow:0 0 12px rgba(255,0,110,.6)}
.cyan{color:#06ffd0;text-shadow:0 0 12px rgba(6,255,208,.6)}
.gold{color:#ffd700;text-shadow:0 0 12px rgba(255,215,0,.5)}
.dot{display:inline-block;width:7px;height:7px;border-radius:50%;background:#06ffd0;box-shadow:0 0 8px #06ffd0;margin-right:5px;vertical-align:middle}
footer{margin-top:18px;font-size:.5rem;letter-spacing:4px;color:rgba(255,255,255,.12);text-align:center;text-transform:uppercase}
</style></head><body>
<div class="nav"><a href="/">⬡ MODES</a><span>COSMIC-S3</span></div>
<div class="glw g1"></div><div class="glw g2"></div><div class="glw g3"></div><div class="glw g4"></div>
<div class="stars">
<div class="s" style="width:2px;height:2px;top:7%;left:13%"></div>
<div class="s" style="width:1px;height:1px;top:11%;left:82%"></div>
<div class="s" style="width:2px;height:2px;top:18%;left:45%"></div>
<div class="s" style="width:1px;height:1px;top:29%;left:7%"></div>
<div class="s" style="width:3px;height:3px;top:36%;left:90%"></div>
<div class="s" style="width:1px;height:1px;top:43%;left:24%"></div>
<div class="s" style="width:2px;height:2px;top:51%;left:71%"></div>
<div class="s" style="width:1px;height:1px;top:60%;left:37%"></div>
<div class="s" style="width:2px;height:2px;top:67%;left:86%"></div>
<div class="s" style="width:1px;height:1px;top:73%;left:17%"></div>
<div class="s" style="width:2px;height:2px;top:81%;left:54%"></div>
<div class="s" style="width:1px;height:1px;top:88%;left:76%"></div>
<div class="s" style="width:3px;height:3px;top:4%;left:61%"></div>
<div class="s" style="width:1px;height:1px;top:25%;left:70%"></div>
<div class="s" style="width:2px;height:2px;top:55%;left:4%"></div>
<div class="s" style="width:1px;height:1px;top:92%;left:40%"></div>
<div class="s" style="width:2px;height:2px;top:16%;left:32%"></div>
<div class="s" style="width:1px;height:1px;top:46%;left:60%"></div>
<div class="s" style="width:2px;height:2px;top:77%;left:9%"></div>
<div class="s" style="width:1px;height:1px;top:34%;left:52%"></div>
</div>
<h1>COSMIC PORTAL</h1>
<p class="sub">ESP32 &middot; C3 &middot; SUPER MINI</p>
<div class="mandala">
  <div class="r r0"></div><div class="d d0"></div>
  <div class="r r1"></div><div class="d d1"></div>
  <div class="r r2"></div><div class="d d2"></div>
  <div class="r r3"></div><div class="d d3"></div>
  <div class="r r4"></div><div class="d d4"></div>
  <div class="r r5"></div>
  <div class="sp" style="transform:rotate(0deg)"></div>
  <div class="sp" style="transform:rotate(30deg)"></div>
  <div class="sp" style="transform:rotate(60deg)"></div>
  <div class="sp" style="transform:rotate(90deg)"></div>
  <div class="sp" style="transform:rotate(120deg)"></div>
  <div class="sp" style="transform:rotate(150deg)"></div>
  <div class="hl h1"></div><div class="hl h2"></div>
  <div class="r core"></div>
</div>
<div class="card">
  <div class="lbl">Network</div><div class="val pink">COSMIC-S3</div>
  <div class="lbl">Address</div><div class="val cyan">192.168.4.1</div>
  <div class="lbl">Status</div><div class="val gold"><span class="dot"></span>TRANSMITTING</div>
</div>
<footer>wifi &middot; ap mode &middot; esp32-c3 &middot; 2.4&nbsp;ghz</footer>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char PLASMA_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PLASMA · COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#030008}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(3,0,8,.8);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(131,56,236,.25);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#8338ec;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#c77dff}
.nav span{color:rgba(255,255,255,.18);font-size:.5rem;letter-spacing:3px}
.blob{position:fixed;border-radius:50%;mix-blend-mode:screen}
.b1{width:90vmax;height:90vmax;
background:radial-gradient(circle,rgba(140,0,255,.9) 0%,rgba(90,0,210,.45) 30%,transparent 70%);
animation:f1 9s ease-in-out infinite}
.b2{width:80vmax;height:80vmax;
background:radial-gradient(circle,rgba(255,50,0,.85) 0%,rgba(200,20,0,.4) 30%,transparent 70%);
animation:f2 11s ease-in-out infinite}
.b3{width:85vmax;height:85vmax;
background:radial-gradient(circle,rgba(0,180,255,.8) 0%,rgba(0,90,200,.35) 30%,transparent 70%);
animation:f3 13s ease-in-out infinite}
.b4{width:75vmax;height:75vmax;
background:radial-gradient(circle,rgba(0,255,120,.75) 0%,rgba(0,170,70,.3) 30%,transparent 70%);
animation:f4 7s ease-in-out infinite}
.b5{width:70vmax;height:70vmax;
background:radial-gradient(circle,rgba(255,210,0,.8) 0%,rgba(210,100,0,.35) 30%,transparent 70%);
animation:f5 15s ease-in-out infinite}
.b6{width:65vmax;height:65vmax;
background:radial-gradient(circle,rgba(255,0,140,.8) 0%,rgba(180,0,80,.35) 30%,transparent 70%);
animation:f6 8s ease-in-out infinite}
@keyframes f1{0%,100%{transform:translate(-15%,-25%)}25%{transform:translate(45%,25%)}50%{transform:translate(55%,-35%)}75%{transform:translate(5%,55%)}}
@keyframes f2{0%,100%{transform:translate(55%,55%)}25%{transform:translate(-25%,15%)}50%{transform:translate(15%,-45%)}75%{transform:translate(65%,-15%)}}
@keyframes f3{0%,100%{transform:translate(15%,-45%)}33%{transform:translate(-35%,45%)}66%{transform:translate(65%,25%)}}
@keyframes f4{0%,100%{transform:translate(45%,15%)}25%{transform:translate(-15%,55%)}50%{transform:translate(25%,-35%)}75%{transform:translate(-25%,-15%)}}
@keyframes f5{0%,100%{transform:translate(-25%,35%)}33%{transform:translate(55%,-25%)}66%{transform:translate(5%,65%)}}
@keyframes f6{0%,100%{transform:translate(35%,-35%)}20%{transform:translate(-35%,5%)}60%{transform:translate(45%,45%)}80%{transform:translate(5%,-45%)}}
.lbl{position:fixed;bottom:18px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.52rem;letter-spacing:6px;
color:rgba(255,255,255,.25);text-transform:uppercase;z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">⬡ MODES</a><span>PLASMA</span></div>
<div class="blob b1"></div>
<div class="blob b2"></div>
<div class="blob b3"></div>
<div class="blob b4"></div>
<div class="blob b5"></div>
<div class="blob b6"></div>
<div class="lbl">PLASMA &middot; CSS ANIMATED &middot; COSMIC-S3</div>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char FRACTAL_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>FRACTAL · COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000}
canvas{display:block;width:100%;height:100%}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,0,.7);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(6,255,208,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#06ffd0;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#80ffe8}
.nav span{color:rgba(255,255,255,.18);font-size:.5rem;letter-spacing:3px}
#overlay{position:fixed;inset:0;display:flex;flex-direction:column;
align-items:center;justify-content:center;z-index:50;
font-family:'Courier New',monospace;pointer-events:none}
.ov-title{font-size:clamp(.8rem,3vw,1.1rem);letter-spacing:6px;
color:#06ffd0;text-shadow:0 0 15px rgba(6,255,208,.7);margin-bottom:12px}
.ov-sub{font-size:.55rem;letter-spacing:4px;color:rgba(6,255,208,.45)}
.lbl{position:fixed;bottom:18px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.52rem;letter-spacing:5px;
color:rgba(255,255,255,.22);text-transform:uppercase;z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">⬡ MODES</a><span>FRACTAL</span></div>
<canvas id="c"></canvas>
<div id="overlay">
  <div class="ov-title">COMPUTING</div>
  <div class="ov-sub">JULIA SET &middot; C = &minus;0.7269 + 0.1889i</div>
</div>
<div class="lbl">JULIA SET &middot; ESCAPE TIME &middot; COSMIC-S3</div>
<script>
(function(){
  var canvas = document.getElementById('c');
  var W = canvas.width  = window.innerWidth;
  var H = canvas.height = window.innerHeight;
  var ctx = canvas.getContext('2d');
  var img = ctx.createImageData(W, H);
  var d = img.data;
  var cr = -0.7269, ci = 0.1889;
  var MAX = 120;
  var scale = Math.min(W, H) / 3.0;

  function hsl2rgb(h, s, l) {
    s /= 100; l /= 100;
    var a = s * Math.min(l, 1 - l);
    function f(n) {
      var k = (n + h / 30) % 12;
      return l - a * Math.max(-1, Math.min(k - 3, Math.min(9 - k, 1)));
    }
    return [Math.round(f(0)*255), Math.round(f(8)*255), Math.round(f(4)*255)];
  }

  // Render in chunks to avoid blocking the browser
  var py = 0;
  function renderChunk() {
    var end = Math.min(py + 20, H);
    for (; py < end; py++) {
      for (var px = 0; px < W; px++) {
        var zx = (px - W * 0.5) / scale;
        var zy = (py - H * 0.5) / scale;
        var i = 0;
        while (zx*zx + zy*zy < 4 && i < MAX) {
          var tx = zx*zx - zy*zy + cr;
          zy = 2*zx*zy + ci;
          zx = tx;
          i++;
        }
        var idx = (py * W + px) * 4;
        if (i === MAX) {
          d[idx] = d[idx+1] = d[idx+2] = 0;
        } else {
          var t = i / MAX;
          var rgb = hsl2rgb((200 + t * 300) % 360, 100, 20 + t * 55);
          d[idx] = rgb[0]; d[idx+1] = rgb[1]; d[idx+2] = rgb[2];
        }
        d[idx+3] = 255;
      }
    }
    ctx.putImageData(img, 0, 0);
    if (py < H) {
      requestAnimationFrame(renderChunk);
    } else {
      document.getElementById('overlay').style.display = 'none';
    }
  }
  requestAnimationFrame(renderChunk);
})();
</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char MATRIX_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MATRIX &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,5,0,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(0,255,65,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#00ff41;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#afffaf}
.nav span{color:rgba(0,255,65,.3);font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;font-family:'Courier New',monospace;
font-size:.5rem;letter-spacing:6px;color:rgba(0,255,65,.2);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>MATRIX</span></div>
<canvas id="c"></canvas>
<div class="lbl">DIGITAL RAIN &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var fs=14,cols=W/fs|0,drops=[];
for(var i=0;i<cols;i++)drops[i]=Math.random()*H/fs|0;
var ch='0123456789ABCDEFGHIJKLabcdefghijk@#$%&*<>{}|'
  +'&#xFF66;&#xFF67;&#xFF68;&#xFF69;&#xFF6A;&#xFF6B;&#xFF6C;&#xFF6D;&#xFF6E;&#xFF6F;&#xFF71;&#xFF72;&#xFF73;&#xFF74;&#xFF75;'
  +'&#xFF76;&#xFF77;&#xFF78;&#xFF79;&#xFF7A;&#xFF7B;&#xFF7C;&#xFF7D;&#xFF7E;&#xFF7F;&#xFF80;&#xFF81;&#xFF82;&#xFF83;&#xFF84;'
  +'&#xFF85;&#xFF86;&#xFF87;&#xFF88;&#xFF89;&#xFF8A;&#xFF8B;&#xFF8C;&#xFF8D;&#xFF8E;&#xFF8F;&#xFF90;&#xFF91;&#xFF92;&#xFF93;'
  +'&#xFF94;&#xFF95;&#xFF96;&#xFF97;&#xFF98;&#xFF99;&#xFF9A;&#xFF9B;&#xFF9C;&#xFF9D;';
function draw(){
  ctx.fillStyle='rgba(0,0,0,0.05)';
  ctx.fillRect(0,0,W,H);
  ctx.font=fs+'px monospace';
  for(var i=0;i<cols;i++){
    ctx.fillStyle='#afffaf';
    ctx.fillText(ch[Math.random()*ch.length|0],i*fs,drops[i]*fs);
    if(drops[i]*fs>H&&Math.random()>.975)drops[i]=0;
    drops[i]++;
  }
}
setInterval(draw,50);
})();
</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char CYBER_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CYBER RAIN &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(8,0,8,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(255,0,170,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#ff00aa;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#ff80d5}
.nav span{color:rgba(255,0,170,.3);font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;font-family:'Courier New',monospace;
font-size:.5rem;letter-spacing:6px;color:rgba(255,0,170,.2);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>CYBER RAIN</span></div>
<canvas id="c"></canvas>
<div class="lbl">NEON MATRIX &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var fs=14,cols=W/fs|0,drops=[],clr=[];
var pal=['#ff00aa','#aa00ff','#00ffbb','#ff6600','#00ccff','#ffff00','#ff0055','#00ff88'];
for(var i=0;i<cols;i++){drops[i]=Math.random()*H/fs|0;clr[i]=pal[Math.random()*pal.length|0];}
var ch='0123456789ABCDEFGHIJKLabcdefghijk@#$%&*<>{}|'
  +'&#xFF66;&#xFF67;&#xFF68;&#xFF69;&#xFF6A;&#xFF6B;&#xFF6C;&#xFF6D;&#xFF6E;&#xFF6F;&#xFF71;&#xFF72;&#xFF73;&#xFF74;&#xFF75;'
  +'&#xFF76;&#xFF77;&#xFF78;&#xFF79;&#xFF7A;&#xFF7B;&#xFF7C;&#xFF7D;&#xFF7E;&#xFF7F;&#xFF80;&#xFF81;&#xFF82;&#xFF83;&#xFF84;'
  +'&#xFF85;&#xFF86;&#xFF87;&#xFF88;&#xFF89;&#xFF8A;&#xFF8B;&#xFF8C;&#xFF8D;&#xFF8E;&#xFF8F;&#xFF90;&#xFF91;&#xFF92;&#xFF93;';
function draw(){
  ctx.fillStyle='rgba(0,0,0,0.05)';
  ctx.fillRect(0,0,W,H);
  ctx.font=fs+'px monospace';
  for(var i=0;i<cols;i++){
    ctx.shadowBlur=9;ctx.shadowColor=clr[i];
    ctx.fillStyle=clr[i];
    ctx.fillText(ch[Math.random()*ch.length|0],i*fs,drops[i]*fs);
    ctx.shadowBlur=0;
    if(drops[i]*fs>H&&Math.random()>.975)drops[i]=0;
    drops[i]++;
  }
}
setInterval(draw,50);
})();
</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char BINARY_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BINARY &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000008}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,15,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(0,136,255,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#44aaff;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#aaddff}
.nav span{color:rgba(0,136,255,.3);font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;font-family:'Courier New',monospace;
font-size:.5rem;letter-spacing:6px;color:rgba(0,136,255,.2);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>BINARY</span></div>
<canvas id="c"></canvas>
<div class="lbl">MACHINE CODE &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var fs=16,cols=W/fs|0,drops=[];
for(var i=0;i<cols;i++)drops[i]=Math.random()*H/fs|0;
function draw(){
  ctx.fillStyle='rgba(0,0,12,0.05)';
  ctx.fillRect(0,0,W,H);
  ctx.font='bold '+fs+'px monospace';
  for(var i=0;i<cols;i++){
    ctx.fillStyle='#55bbff';
    ctx.fillText(Math.random()>.5?'1':'0',i*fs,drops[i]*fs);
    if(drops[i]*fs>H&&Math.random()>.975)drops[i]=0;
    drops[i]++;
  }
}
setInterval(draw,60);
})();
</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char STARFIELD_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>STARFIELD &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000005}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,10,.85);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(180,180,255,.15);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#b4b4ff;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#e0e0ff}
.nav span{color:rgba(180,180,255,.25);font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;font-family:'Courier New',monospace;
font-size:.5rem;letter-spacing:6px;color:rgba(180,180,255,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>STARFIELD</span></div>
<canvas id="c"></canvas>
<div class="lbl">HYPERSPACE JUMP &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var N=200,spd=5,stars=[];
function mk(){return{x:(Math.random()-.5)*W*2,y:(Math.random()-.5)*H*2,z:Math.random()*W};}
for(var i=0;i<N;i++)stars[i]=mk();
var cx=W/2,cy=H/2;
function draw(){
  ctx.fillStyle='rgba(0,0,5,0.22)';
  ctx.fillRect(0,0,W,H);
  for(var i=0;i<N;i++){
    var s=stars[i],pz=s.z;
    s.z-=spd;
    if(s.z<=1){stars[i]=mk();stars[i].z=W;continue;}
    var sx=s.x/s.z*W+cx,sy=s.y/s.z*H+cy;
    var px=s.x/pz*W+cx,py=s.y/pz*H+cy;
    var b=1-s.z/W;
    var v=180+b*75|0;
    ctx.strokeStyle='rgba('+v+','+v+',255,'+b+')';
    ctx.lineWidth=Math.max(0.5,b*2.5);
    ctx.beginPath();ctx.moveTo(px,py);ctx.lineTo(sx,sy);ctx.stroke();
  }
}
setInterval(draw,30);
})();
</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char PARTICLES_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PARTICLES &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#03001a}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(3,0,26,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(255,215,0,.15);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#ffd700;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#ffe96b}
.nav span{color:rgba(255,215,0,.25);font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;font-family:'Courier New',monospace;
font-size:.5rem;letter-spacing:6px;color:rgba(255,215,0,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>PARTICLES</span></div>
<canvas id="c"></canvas>
<div class="lbl">CONSTELLATION MESH &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var N=70,DIST2=130*130,pts=[];
for(var i=0;i<N;i++)pts.push({x:Math.random()*W,y:Math.random()*H,
  vx:(Math.random()-.5)*.65,vy:(Math.random()-.5)*.65,r:Math.random()*2+1});
function draw(){
  ctx.fillStyle='rgba(3,0,26,0.18)';
  ctx.fillRect(0,0,W,H);
  for(var i=0;i<N;i++){
    var p=pts[i];
    p.x+=p.vx;p.y+=p.vy;
    if(p.x<0||p.x>W)p.vx*=-1;
    if(p.y<0||p.y>H)p.vy*=-1;
    for(var j=i+1;j<N;j++){
      var dx=pts[j].x-p.x,dy=pts[j].y-p.y,d2=dx*dx+dy*dy;
      if(d2<DIST2){
        var a=(1-Math.sqrt(d2/DIST2))*.7;
        ctx.strokeStyle='rgba(255,210,60,'+a+')';
        ctx.lineWidth=.7;
        ctx.beginPath();ctx.moveTo(p.x,p.y);ctx.lineTo(pts[j].x,pts[j].y);ctx.stroke();
      }
    }
    ctx.fillStyle='rgba(255,230,100,.9)';
    ctx.beginPath();ctx.arc(p.x,p.y,p.r,0,Math.PI*2);ctx.fill();
  }
}
setInterval(draw,33);
})();
</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char TUNNEL_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TUNNEL &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,0,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(255,80,0,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#ff6622;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#ffaa77}
.nav span{color:rgba(255,80,0,.3);font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;font-family:'Courier New',monospace;
font-size:.5rem;letter-spacing:6px;color:rgba(255,80,0,.2);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>TUNNEL</span></div>
<canvas id="c"></canvas>
<div class="lbl">INFINITE VORTEX &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var t=0,cx=W/2,cy=H/2,R=20;
function draw(){
  ctx.fillStyle='rgba(0,0,0,0.12)';
  ctx.fillRect(0,0,W,H);
  for(var i=R;i>=0;i--){
    var f=i/R;
    var r=Math.pow(f,1.3)*Math.min(W,H)*.55;
    var hue=(t*80+i*(360/R))%360;
    ctx.strokeStyle='hsla('+hue+',100%,58%,'+(0.4+f*.5)+')';
    ctx.lineWidth=1.5;
    ctx.save();ctx.translate(cx,cy);ctx.rotate(t*.9+i*.13);
    ctx.beginPath();ctx.rect(-r,-r,r*2,r*2);ctx.stroke();
    ctx.restore();
  }
  t+=0.026;
}
setInterval(draw,30);
})();
</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char MFIRE_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MATRIX FIRE &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#050000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(8,0,0,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(255,80,0,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#ff6600;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#ffaa44}
.nav span{color:#446600;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(255,80,0,.2);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>MATRIX FIRE</span></div>
<canvas id="c"></canvas>
<div class="lbl">FIRE RAIN &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var fs=14,cols=W/fs|0,drops=[];
for(var i=0;i<cols;i++)drops[i]=Math.random()*H/fs|0;
var ch='0123456789ABCDEF@#$%&*<>{}|ｦｧｨｩｪｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝ';

function draw(){
  ctx.fillStyle='rgba(8,0,0,0.05)';
  ctx.fillRect(0,0,W,H);
  ctx.font=fs+'px monospace';
  for(var i=0;i<cols;i++){
    ctx.fillStyle='#ffee30';ctx.fillText(ch[Math.random()*ch.length|0],i*fs,drops[i]*fs);
    if(drops[i]*fs>H&&Math.random()>.975)drops[i]=0;
    drops[i]++;
  }
}
setInterval(draw,50);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char MICE_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MATRIX ICE &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000308}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,12,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(0,180,255,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#44ccff;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#aaeeff}
.nav span{color:#44cc44;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(0,180,255,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>MATRIX ICE</span></div>
<canvas id="c"></canvas>
<div class="lbl">ICE RAIN &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var fs=14,cols=W/fs|0,drops=[];
for(var i=0;i<cols;i++)drops[i]=Math.random()*H/fs|0;
var ch='0123456789ABCDEF@#$%&*<>{}|ｦｧｨｩｪｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝ';

function draw(){
  ctx.fillStyle='rgba(0,0,10,0.05)';
  ctx.fillRect(0,0,W,H);
  ctx.font=fs+'px monospace';
  for(var i=0;i<cols;i++){
    ctx.fillStyle='#c0ffff';ctx.fillText(ch[Math.random()*ch.length|0],i*fs,drops[i]*fs);
    if(drops[i]*fs>H&&Math.random()>.975)drops[i]=0;
    drops[i]++;
  }
}
setInterval(draw,50);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char MSTORM_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MATRIX STORM &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000408}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,12,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(120,100,255,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#9988ff;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#ccbbff}
.nav span{color:#998844;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(120,100,255,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>MATRIX STORM</span></div>
<canvas id="c"></canvas>
<div class="lbl">LIGHTNING RAIN &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var fs=14,cols=W/fs|0,drops=[];
for(var i=0;i<cols;i++)drops[i]=Math.random()*H/fs|0;
var ch='0123456789ABCDEF@#$%&*<>{}|ｦｧｨｩｪｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝ';

function draw(){
  ctx.fillStyle='rgba(0,0,6,0.05)';
  ctx.fillRect(0,0,W,H);
  ctx.font=fs+'px monospace';
  for(var i=0;i<cols;i++){
    ctx.fillStyle=Math.random()<0.04?'#ffffff':'#a0b0ff';ctx.fillText(ch[Math.random()*ch.length|0],i*fs,drops[i]*fs);
    if(drops[i]*fs>H&&Math.random()>.975)drops[i]=0;
    drops[i]++;
  }
}
setInterval(draw,50);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char MBLOOD_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MATRIX BLOOD &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#080000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(10,0,0,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(200,0,0,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#cc2020;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#ff6060}
.nav span{color:#cc2020;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(200,0,0,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>MATRIX BLOOD</span></div>
<canvas id="c"></canvas>
<div class="lbl">BLOOD RAIN &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var fs=14,cols=W/fs|0,drops=[];
for(var i=0;i<cols;i++)drops[i]=Math.random()*H/fs|0;
var ch='0123456789ABCDEF@#$%&*<>{}|ｦｧｨｩｪｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝ';

function draw(){
  ctx.fillStyle='rgba(8,0,0,0.05)';
  ctx.fillRect(0,0,W,H);
  ctx.font=fs+'px monospace';
  for(var i=0;i<cols;i++){
    ctx.fillStyle='#ff2020';ctx.fillText(ch[Math.random()*ch.length|0],i*fs,drops[i]*fs);
    if(drops[i]*fs>H&&Math.random()>.975)drops[i]=0;
    drops[i]++;
  }
}
setInterval(draw,50);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char MGOLD_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MATRIX GOLD &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#050300}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(10,6,0,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(220,170,0,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#ddaa00;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#ffdd44}
.nav span{color:#ddaa00;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(220,170,0,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>MATRIX GOLD</span></div>
<canvas id="c"></canvas>
<div class="lbl">GOLDEN RAIN &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var fs=14,cols=W/fs|0,drops=[];
for(var i=0;i<cols;i++)drops[i]=Math.random()*H/fs|0;
var ch='0123456789ABCDEF@#$%&*<>{}|ｦｧｨｩｪｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝ';

function draw(){
  ctx.fillStyle='rgba(8,4,0,0.05)';
  ctx.fillRect(0,0,W,H);
  ctx.font=fs+'px monospace';
  for(var i=0;i<cols;i++){
    ctx.fillStyle='#ffd700';ctx.fillText(ch[Math.random()*ch.length|0],i*fs,drops[i]*fs);
    if(drops[i]*fs>H&&Math.random()>.975)drops[i]=0;
    drops[i]++;
  }
}
setInterval(draw,50);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char MVOID_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MATRIX VOID &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#e8e8ff}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(220,220,255,.92);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(50,0,150,.15);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#5500cc;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#8844ff}
.nav span{color:#5500cc;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(100,80,200,.3);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>MATRIX VOID</span></div>
<canvas id="c"></canvas>
<div class="lbl">VOID RAIN &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var fs=14,cols=W/fs|0,drops=[];
for(var i=0;i<cols;i++)drops[i]=Math.random()*H/fs|0;
var ch='0123456789ABCDEF@#$%&*<>{}|ｦｧｨｩｪｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝ';
ctx.fillStyle='rgb(232,232,255)';ctx.fillRect(0,0,W,H);
function draw(){
  ctx.fillStyle='rgba(232,232,255,0.12)';
  ctx.fillRect(0,0,W,H);
  ctx.font=fs+'px monospace';
  for(var i=0;i<cols;i++){
    ctx.fillStyle='#0a0028';
    ctx.fillText(ch[Math.random()*ch.length|0],i*fs,drops[i]*fs);
    if(drops[i]*fs>H&&Math.random()>.975)drops[i]=0;
    drops[i]++;
  }
}
setInterval(draw,50);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char MPHANTOM_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MATRIX PHANTOM &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,0,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(180,255,220,.15);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#80ffcc;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#bbffee}
.nav span{color:#8044cc;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(180,255,220,.15);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>MATRIX PHANTOM</span></div>
<canvas id="c"></canvas>
<div class="lbl">GHOST RAIN &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var fs=14,cols=W/fs|0,drops=[];
for(var i=0;i<cols;i++)drops[i]=Math.random()*H/fs|0;
var ch='0123456789ABCDEF@#$%&*<>{}|ｦｧｨｩｪｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝ';

function draw(){
  ctx.fillStyle='rgba(0,0,0,0.04)';
  ctx.fillRect(0,0,W,H);
  ctx.font=fs+'px monospace';
  for(var i=0;i<cols;i++){
    if(Math.random()>0.28){
    ctx.globalAlpha=0.35+Math.random()*0.65;
    ctx.fillStyle='#c0ffe8';
    ctx.fillText(ch[Math.random()*ch.length|0],i*fs,drops[i]*fs);
    ctx.globalAlpha=1;
  }
    if(drops[i]*fs>H&&Math.random()>.975)drops[i]=0;
    drops[i]++;
  }
}
setInterval(draw,50);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char MRIPPLE_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MATRIX RIPPLE &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000508}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,5,10,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(0,220,255,.18);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#00ddff;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#88eeff}
.nav span{color:#00dd44;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(0,220,255,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>MATRIX RIPPLE</span></div>
<canvas id="c"></canvas>
<div class="lbl">RIPPLE RAIN &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var fs=14,cols=W/fs|0,drops=[];
for(var i=0;i<cols;i++)drops[i]=Math.random()*H/fs|0;
var ch='0123456789ABCDEF@#$%&*<>{}|ｦｧｨｩｪｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝ';
var t=0;
function draw(){
  ctx.fillStyle='rgba(0,3,6,0.05)';
  ctx.fillRect(0,0,W,H);
  ctx.font=fs+'px monospace';
  for(var i=0;i<cols;i++){
    var xoff=Math.sin(t*2.2+i*0.28)*6;
    ctx.fillStyle='#50e8ff';
    ctx.fillText(ch[Math.random()*ch.length|0],i*fs+xoff,drops[i]*fs);
    if(drops[i]*fs>H&&Math.random()>.975)drops[i]=0;
    drops[i]++;
  }
  t+=0.016;
}
setInterval(draw,50);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char MGLITCH_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MATRIX GLITCH &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,0,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(0,255,65,.18);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#00ff41;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#afffaf}
.nav span{color:#004441;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(0,255,65,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>MATRIX GLITCH</span></div>
<canvas id="c"></canvas>
<div class="lbl">GLITCH RAIN &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var fs=14,cols=W/fs|0,drops=[];
for(var i=0;i<cols;i++)drops[i]=Math.random()*H/fs|0;
var ch='0123456789ABCDEF@#$%&*<>{}|ｦｧｨｩｪｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝ';
var pal=['#ff0066','#00ffaa','#ff9900','#aa00ff','#00ccff','#ff00ff'];
function draw(){
  ctx.fillStyle='rgba(0,0,0,0.05)';
  ctx.fillRect(0,0,W,H);
  ctx.font=fs+'px monospace';
  for(var i=0;i<cols;i++){
    var glitch=Math.random()<0.04;
    var xoff=glitch?(Math.random()-.5)*24:0;
    ctx.fillStyle=glitch?pal[Math.random()*pal.length|0]:'#00ff41';
    ctx.fillText(ch[Math.random()*ch.length|0],i*fs+xoff,drops[i]*fs);
    if(drops[i]*fs>H&&Math.random()>.975)drops[i]=0;
    drops[i]++;
  }
}
setInterval(draw,50);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char HOPALONG_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HOPALONG &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,0,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(255,100,200,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#ff66cc;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#ffaaee}
.nav span{color:#4466cc;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(255,100,200,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>HOPALONG</span></div>
<canvas id="c"></canvas>
<div class="lbl">HOPALONG ATTRACTOR &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var hx=0,hy=0,ha=7.7,hb=0.4,hc=1,ht=0;
ctx.fillStyle='#000';ctx.fillRect(0,0,W,H);
var cx=W/2,cy=H/2,sc=Math.min(W,H)/4.5;
function draw(){
  ha=7.7+Math.sin(ht)*2.5; hb=0.4+Math.cos(ht*1.3)*.25;
  ctx.fillStyle='rgba(0,0,0,0.012)';ctx.fillRect(0,0,W,H);
  for(var i=0;i<900;i++){
    var nx=hy-(hx<0?-1:1)*Math.sqrt(Math.abs(hb*hx-hc));
    var ny=ha-hx; hx=nx; hy=ny;
    var px=cx+hx*sc,py=cy+hy*sc;
    if(px>=0&&px<W&&py>=0&&py<H){
      var hue=(Math.atan2(hy,hx)/Math.PI*180+180)%360;
      ctx.fillStyle='hsl('+hue+',100%,65%)';
      ctx.fillRect(px|0,py|0,1,1);
    }
  }
  ht+=0.003;
}
setInterval(draw,30);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char INTERFERENCE_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>INTERFERENCE &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,0,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(180,100,255,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#cc88ff;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#eeccff}
.nav span{color:#cc8844;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(180,100,255,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>INTERFERENCE</span></div>
<canvas id="c"></canvas>
<div class="lbl">WAVE INTERFERENCE &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var S=3,IW=W/S|0,IH=H/S|0;
var ofc=document.createElement('canvas');ofc.width=IW;ofc.height=IH;
var octx=ofc.getContext('2d');
var imgd=octx.createImageData(IW,IH);var d=imgd.data;
var t=0;
function draw(){
  var s1x=IW*.35|0,s1y=IH*.5|0,s2x=IW*.65|0,s2y=IH*.5|0;
  for(var py=0;py<IH;py++){
    for(var px=0;px<IW;px++){
      var d1=Math.sqrt((px-s1x)*(px-s1x)+(py-s1y)*(py-s1y));
      var d2=Math.sqrt((px-s2x)*(px-s2x)+(py-s2y)*(py-s2y));
      var w=(Math.sin(d1*.22-t*3)+Math.sin(d2*.22-t*3))*.5;
      var v=(w*.5+.5)*255|0;
      var idx=(py*IW+px)*4;
      d[idx]=v;d[idx+1]=(v*.65)|0;d[idx+2]=(v*.95)|0;d[idx+3]=255;
    }
  }
  octx.putImageData(imgd,0,0);
  ctx.drawImage(ofc,0,0,W,H);
  t+=0.05;
}
setInterval(draw,40);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char VORONOI_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>VORONOI &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,0,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(255,200,50,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#ffcc33;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#ffee88}
.nav span{color:#44cc33;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(255,200,50,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>VORONOI</span></div>
<canvas id="c"></canvas>
<div class="lbl">VORONOI CELLS &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var N=10,t=0;
var seeds=[];
var pal=['#ff006e','#ff6b00','#ffd700','#06ffd0','#3a86ff','#8338ec','#ff4500','#00ffaa','#ff1493','#1e90ff'];
for(var i=0;i<N;i++) seeds.push({ax:.3+Math.random()*.5,ay:.25+Math.random()*.4,px:Math.random()*6.28,py:Math.random()*6.28});
var step=5,S=2,IW=W/S|0,IH=H/S|0;
var ofc=document.createElement('canvas');ofc.width=IW;ofc.height=IH;
var octx=ofc.getContext('2d');
function draw(){
  var sx=[],sy=[];
  for(var i=0;i<N;i++){
    sx[i]=IW*.5+Math.sin(t*seeds[i].ax+seeds[i].px)*IW*.4;
    sy[i]=IH*.5+Math.cos(t*seeds[i].ay+seeds[i].py)*IH*.38;
  }
  for(var py=0;py<IH;py+=step){
    for(var px=0;px<IW;px+=step){
      var mn=1e9,mi=0;
      for(var s=0;s<N;s++){
        var dx=px-sx[s],dy=py-sy[s],d=dx*dx+dy*dy;
        if(d<mn){mn=d;mi=s;}
      }
      octx.fillStyle=pal[mi];
      octx.fillRect(px,py,step,step);
    }
  }
  for(var i=0;i<N;i++){
    octx.fillStyle='#fff';
    octx.beginPath();octx.arc(sx[i],sy[i],4,0,Math.PI*2);octx.fill();
  }
  ctx.drawImage(ofc,0,0,W,H);
  t+=0.012;
}
setInterval(draw,40);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char STRANGE_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>STRANGE ATTRACTOR &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,0,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(100,255,180,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#44ffaa;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#aaffe0}
.nav span{color:#4444aa;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(100,255,180,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>STRANGE ATTRACTOR</span></div>
<canvas id="c"></canvas>
<div class="lbl">CLIFFORD ATTRACTOR &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var sx=0.1,sy=0,st=0;
var a=-1.4,b=1.6,c=1.0,d=0.7;
ctx.fillStyle='#000';ctx.fillRect(0,0,W,H);
var cx=W/2,cy=H/2,sc=Math.min(W,H)/4.2;
function draw(){
  a=-1.4+Math.sin(st*.7)*.5; b=1.6+Math.cos(st*.5)*.4;
  c=1.0+Math.sin(st*.3)*.3;  d=0.7+Math.cos(st*.9)*.3;
  ctx.fillStyle='rgba(0,0,0,0.008)';ctx.fillRect(0,0,W,H);
  for(var i=0;i<3000;i++){
    var nx=Math.sin(a*sy)+c*Math.cos(a*sx);
    var ny=Math.sin(b*sx)+d*Math.cos(b*sy);
    sx=nx; sy=ny;
    var px=cx+sx*sc,py=cy+sy*sc;
    if(px>=0&&px<W&&py>=0&&py<H){
      var hue=(Math.atan2(sy,sx)/Math.PI*180+180)%360;
      ctx.fillStyle='hsla('+hue+',100%,62%,.5)';
      ctx.fillRect(px|0,py|0,1,1);
    }
  }
  st+=0.002;
}
setInterval(draw,30);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char LISSAJOUS_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>LISSAJOUS &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,0,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(255,150,50,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#ff9933;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#ffcc88}
.nav span{color:#449933;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(255,150,50,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>LISSAJOUS</span></div>
<canvas id="c"></canvas>
<div class="lbl">LISSAJOUS FIGURES &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var lt=0;
var cvs=[
  {a:3,b:2,d:1.5708,col:'#ff006e'},
  {a:5,b:4,d:1.047, col:'#ffd700'},
  {a:4,b:3,d:0.785, col:'#06ffd0'},
  {a:7,b:6,d:0.628, col:'#8338ec'},
  {a:5,b:3,d:0.524, col:'#ff6b00'}
];
var cx=W/2,cy=H/2,rx=W*.42,ry=H*.38;
function draw(){
  ctx.fillStyle='rgba(0,0,0,0.06)';ctx.fillRect(0,0,W,H);
  for(var ci=0;ci<cvs.length;ci++){
    var cv=cvs[ci],ph=lt*.18+ci*.5;
    ctx.strokeStyle=cv.col;ctx.lineWidth=1.5;
    ctx.shadowBlur=7;ctx.shadowColor=cv.col;
    ctx.beginPath();
    for(var i=0;i<=360;i++){
      var th=i*Math.PI/180;
      var x=cx+Math.sin(cv.a*th+cv.d+ph)*rx;
      var y=cy+Math.sin(cv.b*th+ph*.7)*ry;
      i===0?ctx.moveTo(x,y):ctx.lineTo(x,y);
    }
    ctx.stroke();ctx.shadowBlur=0;
  }
  lt+=0.015;
}
setInterval(draw,33);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char SIERPINSKI_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SIERPINSKI &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,0,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(255,220,60,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#ffcc33;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#ffee88}
.nav span{color:#44cc33;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(255,220,60,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>SIERPINSKI</span></div>
<canvas id="c"></canvas>
<div class="lbl">CHAOS GAME &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var vx=[W/2,W*.06,W*.94],vy=[H*.04,H*.95,H*.95];
var px=W/2,py=H/2;
var cols=['#ff006e','#06ffd0','#ffd700'];
ctx.fillStyle='#000';ctx.fillRect(0,0,W,H);
function draw(){
  ctx.fillStyle='rgba(0,0,0,0.01)';ctx.fillRect(0,0,W,H);
  for(var i=0;i<5000;i++){
    var v=Math.random()*3|0;
    px=(px+vx[v])/2; py=(py+vy[v])/2;
    ctx.fillStyle=cols[v];
    ctx.fillRect(px|0,py|0,1,1);
  }
}
setInterval(draw,30);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char SPIROGRAPH_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SPIROGRAPH &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,0,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(200,50,255,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#cc33ff;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#ee99ff}
.nav span{color:#cc3344;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(200,50,255,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>SPIROGRAPH</span></div>
<canvas id="c"></canvas>
<div class="lbl">HYPOTROCHOID &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var spt=0;
var cfgs=[
  {R:110,r:67,d:80,col:'#ff006e'},
  {R:120,r:43,d:90,col:'#06ffd0'},
  {R:90, r:71,d:60,col:'#ffd700'},
  {R:105,r:37,d:95,col:'#8338ec'}
];
var cx=W/2,cy=H/2,sc=Math.min(W,H)/320;
function draw(){
  ctx.fillStyle='rgba(0,0,0,0.04)';ctx.fillRect(0,0,W,H);
  for(var ci=0;ci<cfgs.length;ci++){
    var cf=cfgs[ci];
    ctx.strokeStyle=cf.col;ctx.lineWidth=1.2;
    ctx.shadowBlur=5;ctx.shadowColor=cf.col;
    ctx.beginPath();
    for(var i=0;i<=720;i++){
      var th=i*Math.PI/180+spt;
      var x=cx+((cf.R-cf.r)*Math.cos(th)+cf.d*Math.cos((cf.R-cf.r)/cf.r*th))*sc;
      var y=cy+((cf.R-cf.r)*Math.sin(th)-cf.d*Math.sin((cf.R-cf.r)/cf.r*th))*sc;
      i===0?ctx.moveTo(x,y):ctx.lineTo(x,y);
    }
    ctx.stroke();ctx.shadowBlur=0;
  }
  spt+=0.006;
}
setInterval(draw,33);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char BARNSLEY_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BARNSLEY FERN &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,0,.88);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(50,220,80,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#33dd55;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#99ffaa}
.nav span{color:#33dd55;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(50,220,80,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>BARNSLEY FERN</span></div>
<canvas id="c"></canvas>
<div class="lbl">IFS FRACTAL &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var bx=0,by=0,bt=0;
var cx=W/2,base=H*.96,sc=H*.088;
ctx.fillStyle='#000';ctx.fillRect(0,0,W,H);
function draw(){
  ctx.fillStyle='rgba(0,0,0,0.006)';ctx.fillRect(0,0,W,H);
  for(var i=0;i<4000;i++){
    var r=Math.random(),nx,ny;
    if(r<0.01){nx=0;ny=0.16*by;}
    else if(r<0.86){nx=0.85*bx+0.04*by;ny=-0.04*bx+0.85*by+1.6;}
    else if(r<0.93){nx=0.2*bx-0.26*by;ny=0.23*bx+0.22*by+1.6;}
    else{nx=-0.15*bx+0.28*by;ny=0.26*bx+0.24*by+0.44;}
    bx=nx;by=ny;
    var sx=cx+bx*sc,sy=base-by*sc;
    if(sx>=0&&sx<W&&sy>=0&&sy<H){
      ctx.fillStyle='hsl('+(120+Math.sin(bt+by*.5)*40)+',90%,52%)';
      ctx.fillRect(sx|0,sy|0,1,1);
    }
  }
  bt+=0.005;
}
setInterval(draw,30);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char CAMPFIRE_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CAMPFIRE &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#020008}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(2,0,8,.92);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(255,120,0,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#ff8800;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#ffcc66}
.nav span{color:#448800;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(255,120,0,.2);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>CAMPFIRE</span></div>
<canvas id="c"></canvas>
<div class="lbl">FIRE SIMULATION &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var NUM=Math.floor(W/7)+1,t=0;
var fl=[];
for(var i=0;i<NUM;i++) fl.push({cx:i*7+(Math.random()*4-2),bh:H*.22+Math.random()*H*.2,ph:Math.random()*6.28,sp:.05+Math.random()*.1});
function draw(){
  ctx.fillStyle='rgba(2,0,8,0.14)';ctx.fillRect(0,0,W,H);
  for(var i=0;i<fl.length;i++){
    var f=fl[i];
    f.ph+=f.sp;
    var h=f.bh*(.65+Math.sin(f.ph)*.35);
    var w=10+Math.sin(f.ph*1.7)*4;
    var x=f.cx+Math.sin(f.ph*.8)*5;
    var g=ctx.createLinearGradient(x,H,x,H-h);
    g.addColorStop(0,'rgba(255,230,60,1)');
    g.addColorStop(0.2,'rgba(255,120,0,.9)');
    g.addColorStop(0.55,'rgba(200,10,0,.6)');
    g.addColorStop(1,'rgba(60,0,0,0)');
    ctx.fillStyle=g;
    ctx.beginPath();ctx.ellipse(x,H-h*.5,w/2,h/2,0,0,Math.PI*2);ctx.fill();
  }
  t++;
}
setInterval(draw,40);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char RAINDROPS_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RAINDROPS &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000810}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,8,18,.9);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(0,160,255,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#0099ff;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#66ccff}
.nav span{color:#009944;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(0,160,255,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>RAINDROPS</span></div>
<canvas id="c"></canvas>
<div class="lbl">WATER RIPPLES &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var drops=[];
function add(){
  if(drops.length<40) drops.push({x:Math.random()*W,y:Math.random()*H,r:0,mr:40+Math.random()*90,sp:1+Math.random()*2,hue:185+Math.random()*50});
}
function draw(){
  ctx.fillStyle='rgba(0,8,16,0.1)';ctx.fillRect(0,0,W,H);
  if(Math.random()<0.07) add();
  drops=drops.filter(function(d){
    d.r+=d.sp;
    if(d.r>d.mr) return false;
    var a=1-d.r/d.mr;
    ctx.strokeStyle='hsla('+d.hue+',80%,65%,'+a+')';
    ctx.lineWidth=1.2;
    ctx.beginPath();ctx.arc(d.x,d.y,d.r,0,Math.PI*2);ctx.stroke();
    if(d.r>15){
      ctx.strokeStyle='hsla('+d.hue+',60%,45%,'+(a*.5)+')';
      ctx.beginPath();ctx.arc(d.x,d.y,d.r*.65,0,Math.PI*2);ctx.stroke();
    }
    return true;
  });
}
setInterval(draw,33);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char GAMEOFLIFE_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>GAME OF LIFE &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#020010}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(2,0,16,.9);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(0,255,200,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#00ffcc;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#88ffee}
.nav span{color:#0044cc;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(0,255,200,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>GAME OF LIFE</span></div>
<canvas id="c"></canvas>
<div class="lbl">CONWAY CELLULAR AUTOMATON &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var CW=75,CH=130;
var SX=W/CW,SY=H/CH;
var g=new Uint8Array(CW*CH),n=new Uint8Array(CW*CH);
for(var i=0;i<CW*CH;i++) g[i]=Math.random()<0.3?1:0;
var tick=0;
function step(){
  for(var y=0;y<CH;y++) for(var x=0;x<CW;x++){
    var nb=0;
    for(var dy=-1;dy<=1;dy++) for(var dx=-1;dx<=1;dx++){
      if(dx===0&&dy===0) continue;
      nb+=g[((y+dy+CH)%CH)*CW+(x+dx+CW)%CW];
    }
    var c=g[y*CW+x];
    n[y*CW+x]=(c?(nb===2||nb===3):nb===3)?1:0;
  }
  var t=g;g=n;n=t;
  tick++;
  if(tick%250===0) for(var i=0;i<CW*CH;i++) g[i]=Math.random()<0.3?1:0;
}
function draw(){
  ctx.fillStyle='#020010';ctx.fillRect(0,0,W,H);
  for(var y=0;y<CH;y++) for(var x=0;x<CW;x++)
    if(g[y*CW+x]){ctx.fillStyle='#00ffcc';ctx.fillRect(x*SX,y*SY,SX-.4,SY-.4);}
  step();
}
setInterval(draw,80);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char AURORA_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AURORA &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000508}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,5,10,.9);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(0,255,150,.18);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#00ee88;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#88ffcc}
.nav span{color:#00ee88;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(0,255,150,.16);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>AURORA</span></div>
<canvas id="c"></canvas>
<div class="lbl">AURORA BOREALIS &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var at=0;
function draw(){
  ctx.fillStyle='rgba(0,4,10,0.1)';ctx.fillRect(0,0,W,H);
  for(var b=0;b<5;b++){
    var baseY=H*.15+b*H*.13;
    var hue=140+b*30+Math.sin(at*.4)*25;
    ctx.beginPath();ctx.moveTo(0,baseY);
    for(var x=0;x<=W;x+=4){
      var y=baseY+Math.sin(x*.014+at*.9+b*.8)*H*.07+Math.sin(x*.028+at*.6+b*1.3)*H*.035;
      ctx.lineTo(x,y);
    }
    ctx.lineTo(W,H);ctx.lineTo(0,H);ctx.closePath();
    var g=ctx.createLinearGradient(0,baseY-H*.07,0,baseY+H*.09);
    g.addColorStop(0,'hsla('+hue+',100%,68%,0)');
    g.addColorStop(.4,'hsla('+hue+',100%,58%,.28)');
    g.addColorStop(1,'hsla('+hue+',100%,35%,0)');
    ctx.fillStyle=g;ctx.fill();
  }
  ctx.fillStyle='rgba(255,255,255,.55)';
  for(var s=0;s<25;s++){
    ctx.fillRect((s*137.5)%W|0,((s*79.3)%H*.18)|0,1,1);
  }
  at+=0.022;
}
setInterval(draw,33);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char KALEIDOSCOPE_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>KALEIDOSCOPE &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,0,.9);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(255,100,255,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#ff66ff;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#ffaaff}
.nav span{color:#446644;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(255,100,255,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>KALEIDOSCOPE</span></div>
<canvas id="c"></canvas>
<div class="lbl">MIRROR SYMMETRY &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var kt=0,SEGS=8;
var cx=W/2,cy=H/2,R=Math.min(W,H)*.46;
function draw(){
  ctx.fillStyle='rgba(0,0,0,0.04)';ctx.fillRect(0,0,W,H);
  for(var s=0;s<SEGS*2;s++){
    ctx.save();
    ctx.translate(cx,cy);
    ctx.rotate(s*Math.PI/SEGS+(s%2?Math.PI/SEGS:0));
    if(s%2===1) ctx.scale(1,-1);
    for(var i=0;i<9;i++){
      var ang=kt*.35+i*.75;
      var r=R*(.08+i*.1);
      var x=Math.cos(ang)*r,y=Math.sin(ang*1.3)*r*.7;
      var hue=(kt*50+i*40+s*22)%360;
      ctx.fillStyle='hsla('+hue+',100%,62%,.65)';
      ctx.beginPath();ctx.arc(x,y,R*.058,0,Math.PI*2);ctx.fill();
    }
    ctx.restore();
  }
  kt+=0.022;
}
setInterval(draw,33);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char DRAGON_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>DRAGON CURVE &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,0,.9);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(255,160,0,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#ff9900;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#ffcc66}
.nav span{color:#449900;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(255,160,0,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>DRAGON CURVE</span></div>
<canvas id="c"></canvas>
<div class="lbl">L-SYSTEM FRACTAL &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var pts=[];
(function(){
  var order=13,turns=[1];
  for(var i=1;i<order;i++){
    var n=turns.length;turns.push(1);
    for(var j=n-1;j>=0;j--) turns.push(-turns[j]);
  }
  var dir=0,cx=0,cy=0;
  pts.push([cx,cy]);
  var DX=[1,0,-1,0],DY=[0,1,0,-1];
  for(var i=0;i<turns.length;i++){
    cx+=DX[dir];cy+=DY[dir];pts.push([cx,cy]);
    dir=(dir+turns[i]+4)%4;
  }
  cx+=DX[dir];cy+=DY[dir];pts.push([cx,cy]);
  var mnX=Infinity,mxX=-Infinity,mnY=Infinity,mxY=-Infinity;
  for(var i=0;i<pts.length;i++){mnX=Math.min(mnX,pts[i][0]);mxX=Math.max(mxX,pts[i][0]);mnY=Math.min(mnY,pts[i][1]);mxY=Math.max(mxY,pts[i][1]);}
  var sc=Math.min(W/(mxX-mnX)*.85,H/(mxY-mnY)*.85);
  var ox=W/2-(mxX+mnX)/2*sc,oy=H/2-(mxY+mnY)/2*sc;
  for(var i=0;i<pts.length;i++) pts[i]=[pts[i][0]*sc+ox,pts[i][1]*sc+oy];
})();
var prog=0,N=pts.length;
function draw(){
  ctx.fillStyle='rgba(0,0,0,0.04)';ctx.fillRect(0,0,W,H);
  prog=(prog+N/90)%N;
  var end=prog|0;
  ctx.lineWidth=1;ctx.shadowBlur=4;ctx.shadowColor='#ff9900';
  ctx.beginPath();ctx.moveTo(pts[0][0],pts[0][1]);
  for(var i=1;i<=end;i++){
    ctx.strokeStyle='hsl('+(i/N*240|0)+',100%,55%)';
    ctx.lineTo(pts[i][0],pts[i][1]);
  }
  ctx.stroke();ctx.shadowBlur=0;
}
setInterval(draw,33);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char LAVA2_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>LAVA LAMP &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#0a0000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(8,0,0,.9);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(255,80,20,.2);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#ff5500;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#ff9966}
.nav span{color:#445500;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(255,80,20,.18);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>LAVA LAMP</span></div>
<canvas id="c"></canvas>
<div class="lbl">METABALLS &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var S=3,IW=W/S|0,IH=H/S|0;
var ofc=document.createElement('canvas');ofc.width=IW;ofc.height=IH;
var octx=ofc.getContext('2d');
var imgd=octx.createImageData(IW,IH);var d=imgd.data;
var blobs=[];
for(var i=0;i<7;i++) blobs.push({x:Math.random()*IW,y:Math.random()*IH,vx:(Math.random()-.5)*.6,vy:(Math.random()-.5)*.6,r:50+Math.random()*30});
function draw(){
  for(var i=0;i<blobs.length;i++){
    blobs[i].x+=blobs[i].vx;blobs[i].y+=blobs[i].vy;
    if(blobs[i].x<0||blobs[i].x>IW) blobs[i].vx*=-1;
    if(blobs[i].y<0||blobs[i].y>IH) blobs[i].vy*=-1;
  }
  for(var py=0;py<IH;py++){
    for(var px=0;px<IW;px++){
      var v=0;
      for(var i=0;i<blobs.length;i++){
        var dx=px-blobs[i].x,dy=py-blobs[i].y;
        v+=blobs[i].r*blobs[i].r/(dx*dx+dy*dy+1);
      }
      var idx=(py*IW+px)*4;
      if(v>1.8){
        var h=Math.min(1,(v-1.8)*.6);
        d[idx]=255;d[idx+1]=(h*140)|0;d[idx+2]=0;d[idx+3]=255;
      } else if(v>0.9){
        var e=(v-.9)/.9;
        d[idx]=(e*160)|0;d[idx+1]=0;d[idx+2]=0;d[idx+3]=255;
      } else {
        d[idx]=10;d[idx+1]=0;d[idx+2]=0;d[idx+3]=255;
      }
    }
  }
  octx.putImageData(imgd,0,0);ctx.drawImage(ofc,0,0,W,H);
}
setInterval(draw,40);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char NOISE_HTML[] = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>NOISE FIELD &middot; COSMIC-S3</title><style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;background:#000}
canvas{display:block;position:fixed;inset:0}
.nav{position:fixed;top:0;left:0;right:0;padding:9px 16px;
background:rgba(0,0,0,.9);backdrop-filter:blur(6px);
border-bottom:1px solid rgba(200,200,255,.18);z-index:99;
display:flex;align-items:center;justify-content:space-between;
font-family:'Courier New',monospace}
.nav a{color:#aaaaff;text-decoration:none;font-size:.62rem;letter-spacing:3px}
.nav a:hover{color:#ddddff}
.nav span{color:#aaaa44;font-size:.5rem;letter-spacing:3px}
.lbl{position:fixed;bottom:14px;width:100%;text-align:center;
font-family:'Courier New',monospace;font-size:.5rem;letter-spacing:6px;
color:rgba(200,200,255,.15);z-index:10;pointer-events:none}
</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>NOISE FIELD</span></div>
<canvas id="c"></canvas>
<div class="lbl">ANIMATED SINE NOISE &middot; COSMIC-S3</div>
<script>
(function(){
var c=document.getElementById('c');
var W=c.width=window.innerWidth,H=c.height=window.innerHeight;
var ctx=c.getContext('2d');
var S=3,IW=W/S|0,IH=H/S|0;
var ofc=document.createElement('canvas');ofc.width=IW;ofc.height=IH;
var octx=ofc.getContext('2d');
var imgd=octx.createImageData(IW,IH);var d=imgd.data;
var nt=0;
function draw(){
  for(var py=0;py<IH;py++){
    for(var px=0;px<IW;px++){
      var v=Math.sin(px*.14+nt)+Math.sin(py*.1+nt*.7)
           +Math.sin((px+py)*.07+nt*.5)+Math.sin((px-py)*.055+nt*.35)
           +Math.sin(Math.sqrt(px*px+py*py)*.09+nt);
      v=v/5;
      var n=v*.5+.5;
      var idx=(py*IW+px)*4;
      d[idx]  =(Math.sin(n*6.2+nt)*.5+.5)*255|0;
      d[idx+1]=(Math.cos(n*5.1+nt*1.3)*.5+.5)*255|0;
      d[idx+2]=(Math.sin(n*7.3-nt*.9)*.5+.5)*255|0;
      d[idx+3]=255;
    }
  }
  octx.putImageData(imgd,0,0);ctx.drawImage(ofc,0,0,W,H);
  nt+=0.04;
}
setInterval(draw,33);
})();
</script></body></html>

)EOF";

// ---------------------------------------------------------------------------
static const char SNAKE_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no"><title>SNAKE · COSMIC-S3</title>
<style>*{margin:0;padding:0;box-sizing:border-box}body{background:#000;color:#0f0;font-family:monospace;display:flex;flex-direction:column;align-items:center;height:100vh;overflow:hidden}
.nav{background:#001100;border-bottom:1px solid #0f0;padding:6px 12px;width:100%;display:flex;justify-content:space-between;align-items:center;font-size:12px}.nav a{color:#0f0;text-decoration:none}
canvas{display:block;image-rendering:pixelated;border:1px solid #0f0;box-shadow:0 0 14px #0f0;margin-top:4px}
#score{font-size:13px;color:#0f0;letter-spacing:2px}
#over{display:none;position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);background:rgba(0,16,0,0.97);border:1px solid #0f0;padding:22px 28px;text-align:center;z-index:10;box-shadow:0 0 20px #0f0}
#over h2{color:#0f0;letter-spacing:4px;margin-bottom:10px;font-size:18px}#over p{color:#0a0;margin-bottom:18px;font-size:13px;letter-spacing:2px}
#over button{background:#000;border:1px solid #0f0;color:#0f0;padding:10px 24px;font-family:monospace;font-size:14px;cursor:pointer;letter-spacing:3px}#over button:hover{background:#0f0;color:#000}
#dpad{display:grid;grid-template-columns:repeat(3,50px);grid-template-rows:repeat(3,50px);gap:4px;margin:8px auto}
.btn{background:#001100;border:1px solid #0f0;color:#0f0;font-size:20px;display:flex;align-items:center;justify-content:center;border-radius:6px;user-select:none;touch-action:manipulation;cursor:pointer}.btn:active{background:#0f0;color:#000}</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span id="score">SCORE: 0 &nbsp; HI: 0</span><span>SNAKE</span></div>
<canvas id="c"></canvas>
<div id="over"><h2>GAME OVER</h2><p id="fs">SCORE: 0</p><button onclick="init()">&#x25BA; RESTART</button></div>
<div id="dpad"><div></div><div class="btn" id="bu">&#x25B2;</div><div></div><div class="btn" id="bl">&#x25C4;</div><div></div><div class="btn" id="br">&#x25BA;</div><div></div><div class="btn" id="bd">&#x25BC;</div><div></div></div>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
const CELL=16,COLS=22,ROWS=20;
C.width=COLS*CELL;C.height=ROWS*CELL;
let snake,dir,nd,food,score,hi=0,tid,alive,animId;
function rand(n){return Math.floor(Math.random()*n)}
function spawn(){let x,y;do{x=rand(COLS);y=rand(ROWS)}while(snake.some(s=>s[0]==x&&s[1]==y));return[x,y]}
function init(){
  snake=[[11,10],[10,10],[9,10],[8,10]];dir=[1,0];nd=[1,0];
  food=spawn();score=0;alive=true;
  document.getElementById('over').style.display='none';
  clearInterval(tid);tid=setInterval(tick,135);
  cancelAnimationFrame(animId);loop();
}
function tick(){
  if(!alive)return;
  dir=[...nd];
  const h=[snake[0][0]+dir[0],snake[0][1]+dir[1]];
  if(h[0]<0||h[0]>=COLS||h[1]<0||h[1]>=ROWS||snake.some(s=>s[0]==h[0]&&s[1]==h[1])){
    alive=false;clearInterval(tid);
    if(score>hi)hi=score;
    document.getElementById('over').style.display='block';
    document.getElementById('fs').textContent='SCORE: '+score+(score>0&&score==hi?' \u2605 NEW HI':'');
    return;
  }
  snake.unshift(h);
  if(h[0]==food[0]&&h[1]==food[1]){
    score++;hi=Math.max(hi,score);
    document.getElementById('score').textContent='SCORE: '+score+' \u00A0 HI: '+hi;
    food=spawn();
    if(score%5==0){clearInterval(tid);tid=setInterval(tick,Math.max(55,135-score*5));}
  }else snake.pop();
}
function loop(){
  draw();animId=requestAnimationFrame(loop);
}
function draw(){
  ctx.fillStyle='#000';ctx.fillRect(0,0,C.width,C.height);
  ctx.fillStyle='#001600';
  for(let x=0;x<COLS;x++)for(let y=0;y<ROWS;y++)ctx.fillRect(x*CELL+7,y*CELL+7,2,2);
  const t=Date.now()/300;
  const r=5+Math.sin(t)*1.5;
  const fx=food[0]*CELL+CELL/2,fy=food[1]*CELL+CELL/2;
  const g=ctx.createRadialGradient(fx,fy,0,fx,fy,r+4);
  g.addColorStop(0,'#ff44aa');g.addColorStop(1,'transparent');
  ctx.fillStyle=g;ctx.beginPath();ctx.arc(fx,fy,r+4,0,Math.PI*2);ctx.fill();
  ctx.fillStyle='#ff2288';ctx.beginPath();ctx.arc(fx,fy,r,0,Math.PI*2);ctx.fill();
  snake.forEach((s,i)=>{
    const bright=Math.max(0,1-i*0.035);
    const gb=Math.floor(bright*255),gg=Math.floor(0x22+bright*0xdd);
    ctx.fillStyle='rgb(0,'+gg+','+gb+')';
    ctx.fillRect(s[0]*CELL+1,s[1]*CELL+1,CELL-2,CELL-2);
    if(i==0){
      ctx.fillStyle='#000';
      const ey=s[1]*CELL+4,ex1=s[0]*CELL+4,ex2=s[0]*CELL+CELL-7;
      ctx.fillRect(ex1,ey,3,3);ctx.fillRect(ex2,ey,3,3);
    }
  });
}
document.addEventListener('keydown',e=>{
  const m={ArrowUp:[0,-1],ArrowDown:[0,1],ArrowLeft:[-1,0],ArrowRight:[1,0],w:[0,-1],s:[0,1],a:[-1,0],d:[1,0]};
  const k=m[e.key];if(k&&!(k[0]==-dir[0]&&k[1]==-dir[1])){nd=k;e.preventDefault();}
});
function setDir(d){if(!(d[0]==-dir[0]&&d[1]==-dir[1]))nd=d;}
['bu','bd','bl','br'].forEach((id,i)=>{
  const dirs=[[0,-1],[0,1],[-1,0],[1,0]];
  const el=document.getElementById(id);
  el.addEventListener('touchstart',e=>{setDir(dirs[i]);e.preventDefault();},{passive:false});
  el.addEventListener('click',()=>setDir(dirs[i]));
});
let tx=0,ty=0;
C.addEventListener('touchstart',e=>{tx=e.touches[0].clientX;ty=e.touches[0].clientY;},{passive:true});
C.addEventListener('touchend',e=>{
  const dx=e.changedTouches[0].clientX-tx,dy=e.changedTouches[0].clientY-ty;
  if(Math.abs(dx)>Math.abs(dy)){setDir(dx>20?[1,0]:[-1,0]);}else{setDir(dy>20?[0,1]:[0,-1]);}
},{passive:true});
init();
</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char BREAKOUT_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no"><title>BREAKOUT · COSMIC-S3</title><style>*{margin:0;padding:0;box-sizing:border-box}body{background:#000;color:#0ff;font-family:monospace;display:flex;flex-direction:column;align-items:center;height:100vh;overflow:hidden}.nav{background:#000a11;border-bottom:1px solid #0ff;padding:6px 12px;width:100%;display:flex;align-items:center;justify-content:space-between;font-size:12px}.nav a{color:#0ff;text-decoration:none}canvas{display:block;border:1px solid rgba(0,255,255,.3);box-shadow:0 0 14px rgba(0,255,255,.4);margin-top:4px}
#over{display:none;position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);background:rgba(0,10,17,.97);border:1px solid #0ff;padding:22px 28px;text-align:center;z-index:10;box-shadow:0 0 20px #0ff}#over h2{color:#0ff;letter-spacing:4px;margin-bottom:10px}#over p{color:#0aa;margin-bottom:18px;font-size:13px;letter-spacing:2px}#over button{background:#000;border:1px solid #0ff;color:#0ff;padding:10px 24px;font-family:monospace;font-size:14px;cursor:pointer;letter-spacing:3px}#over button:hover{background:#0ff;color:#000}</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span id="sc">SCORE: 0 &#x2665;&#x2665;&#x2665;</span><span>BREAKOUT</span></div><canvas id="c"></canvas>
<div id="over"><h2 id="ot">GAME OVER</h2><p id="os">SCORE: 0</p><button onclick="init()">&#x25BA; PLAY</button></div>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
const LW=320,LH=440;C.width=LW;C.height=LH;
const sf=Math.min((window.innerWidth-8)/LW,(window.innerHeight-58)/LH);
C.style.width=Math.floor(LW*sf)+'px';C.style.height=Math.floor(LH*sf)+'px';
const ROWS=6,COLS=8,BW=36,BH=14,GAP=3,PW=58,PH=10,BALL=6;
const BCOL=['#ff006e','#ff6b00','#ffd700','#06ffd0','#3a86ff','#8338ec'];
let px,bx,by,vx,vy,lives,score,hi=0,bricks,state,aid;
function rrect(x,y,w,h,r){ctx.beginPath();ctx.moveTo(x+r,y);ctx.lineTo(x+w-r,y);ctx.arcTo(x+w,y,x+w,y+r,r);ctx.lineTo(x+w,y+h-r);ctx.arcTo(x+w,y+h,x+w-r,y+h,r);ctx.lineTo(x+r,y+h);ctx.arcTo(x,y+h,x,y+h-r,r);ctx.lineTo(x,y+r);ctx.arcTo(x,y,x+r,y,r);ctx.closePath();}
function mkBricks(){
  const b=[],ox=(LW-COLS*(BW+GAP)+GAP)/2;
  for(let r=0;r<ROWS;r++)for(let c=0;c<COLS;c++)b.push({x:ox+c*(BW+GAP),y:42+r*(BH+GAP),a:true,col:BCOL[r]});
  return b;
}
function init(){
  px=LW/2;bx=LW/2;by=LH-75;
  const a=-Math.PI/2+(Math.random()-.5)*.7;vx=Math.cos(a)*4;vy=Math.sin(a)*4;
  score=0;lives=3;bricks=mkBricks();state='play';
  document.getElementById('over').style.display='none';
  document.getElementById('sc').textContent='SCORE: 0 \u2665\u2665\u2665';
  cancelAnimationFrame(aid);aid=requestAnimationFrame(loop);
}
function loop(){update();draw();if(state==='play')aid=requestAnimationFrame(loop);}
function update(){
  bx+=vx;by+=vy;
  if(bx<BALL){bx=BALL;vx=Math.abs(vx);}
  if(bx>LW-BALL){bx=LW-BALL;vx=-Math.abs(vx);}
  if(by<BALL){by=BALL;vy=Math.abs(vy);}
  const py=LH-30;
  if(by+BALL>py&&by-BALL<py+PH&&bx>px-PW/2&&bx<px+PW/2){
    by=py-BALL;vy=-Math.abs(vy);vx+=(bx-px)*.06;
    const sp=Math.sqrt(vx*vx+vy*vy),ts=Math.max(4,Math.min(7,sp));vx=vx/sp*ts;vy=vy/sp*ts;
  }
  if(by>LH+10){
    lives--;if(lives<=0){state='over';showEnd(false);return;}
    bx=px;by=LH-75;const a=-Math.PI/2+(Math.random()-.5)*.7;vx=Math.cos(a)*4;vy=Math.sin(a)*4;
    document.getElementById('sc').textContent='SCORE: '+score+' '+'\u2665'.repeat(lives);
  }
  for(const b of bricks){
    if(!b.a)continue;
    if(bx+BALL>b.x&&bx-BALL<b.x+BW&&by+BALL>b.y&&by-BALL<b.y+BH){
      b.a=false;score+=10;if(score>hi)hi=score;
      document.getElementById('sc').textContent='SCORE: '+score+' '+'\u2665'.repeat(lives);
      if(bx>b.x+2&&bx<b.x+BW-2)vy=-vy;else vx=-vx;break;
    }
  }
  if(bricks.every(b=>!b.a)){state='over';showEnd(true);}
}
function showEnd(w){
  document.getElementById('ot').textContent=w?'YOU WIN!':'GAME OVER';
  document.getElementById('os').textContent='SCORE: '+score+(score>0&&score==hi?' \u2605 HI':'');
  document.getElementById('over').style.display='block';
}
function draw(){
  ctx.fillStyle='#000';ctx.fillRect(0,0,LW,LH);
  ctx.fillStyle='rgba(0,255,255,.025)';for(let x=0;x<LW;x+=14)for(let y=0;y<LH;y+=14)ctx.fillRect(x,y,1,1);
  bricks.forEach(b=>{
    if(!b.a)return;
    ctx.fillStyle=b.col;rrect(b.x,b.y,BW,BH,3);ctx.fill();
    ctx.fillStyle='rgba(255,255,255,.22)';ctx.fillRect(b.x+2,b.y+2,BW-4,3);
  });
  const py=LH-30,pg=ctx.createLinearGradient(px-PW/2,0,px+PW/2,0);
  pg.addColorStop(0,'#006a8a');pg.addColorStop(.5,'#00e5ff');pg.addColorStop(1,'#006a8a');
  ctx.fillStyle=pg;rrect(px-PW/2,py,PW,PH,4);ctx.fill();
  const bg=ctx.createRadialGradient(bx,by,0,bx,by,BALL*3);
  bg.addColorStop(0,'#fff');bg.addColorStop(.45,'#0ff');bg.addColorStop(1,'transparent');
  ctx.fillStyle=bg;ctx.beginPath();ctx.arc(bx,by,BALL*3,0,Math.PI*2);ctx.fill();
  ctx.fillStyle='#fff';ctx.beginPath();ctx.arc(bx,by,BALL*.7,0,Math.PI*2);ctx.fill();
  for(let i=0;i<lives;i++){ctx.fillStyle='#ff006e';ctx.beginPath();ctx.arc(10+i*16,LH-12,4,0,Math.PI*2);ctx.fill();}
}
document.addEventListener('mousemove',e=>{const r=C.getBoundingClientRect();px=Math.max(PW/2,Math.min(LW-PW/2,(e.clientX-r.left)/sf));});
document.addEventListener('keydown',e=>{if(e.key=='ArrowLeft')px=Math.max(PW/2,px-18);if(e.key=='ArrowRight')px=Math.min(LW-PW/2,px+18);});
document.addEventListener('touchmove',e=>{e.preventDefault();const r=C.getBoundingClientRect();px=Math.max(PW/2,Math.min(LW-PW/2,(e.touches[0].clientX-r.left)/sf));},{passive:false});
init();
</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char TETRIS_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no"><title>TETRIS · COSMIC-S3</title><style>*{margin:0;padding:0;box-sizing:border-box}body{background:#000;color:#c77dff;font-family:monospace;display:flex;flex-direction:column;align-items:center;height:100vh;overflow:hidden;gap:3px}.nav{background:#050011;border-bottom:1px solid #8338ec;padding:6px 12px;width:100%;display:flex;align-items:center;justify-content:space-between;font-size:12px}.nav a{color:#c77dff;text-decoration:none}.row{display:flex;align-items:flex-start;gap:8px}.side{display:flex;flex-direction:column;align-items:center;gap:4px;padding-top:2px;font-size:11px;letter-spacing:1px;color:rgba(199,125,255,.65)}canvas{display:block;border:1px solid rgba(131,56,236,.5);box-shadow:0 0 12px rgba(131,56,236,.35)}#nc{border:1px solid rgba(131,56,236,.3)}#dpad{display:flex;gap:4px;margin:4px 0}.btn{background:#05001a;border:1px solid #8338ec;color:#c77dff;font-size:18px;width:52px;height:44px;display:flex;align-items:center;justify-content:center;border-radius:6px;user-select:none;touch-action:manipulation;cursor:pointer}.btn:active{background:#8338ec;color:#fff}
#over{display:none;position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);background:rgba(5,0,17,.97);border:1px solid #8338ec;padding:22px 28px;text-align:center;z-index:10;box-shadow:0 0 20px #8338ec}#over h2{color:#c77dff;letter-spacing:4px;margin-bottom:10px}#over p{color:#9055bb;margin-bottom:18px;font-size:13px;letter-spacing:2px}#over button{background:#000;border:1px solid #8338ec;color:#c77dff;padding:10px 24px;font-family:monospace;font-size:14px;cursor:pointer;letter-spacing:3px}#over button:hover{background:#8338ec;color:#fff}</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span id="sc">SCORE: 0</span><span>TETRIS</span></div>
<div class="row"><canvas id="c"></canvas><div class="side">
<span>NEXT</span><canvas id="nc"></canvas>
<span style="margin-top:6px">LVL</span><span id="lv">0</span>
<span>LINES</span><span id="ln">0</span></div></div>
<div id="dpad">
<div class="btn" id="bl">&#x25C4;</div>
<div class="btn" id="bd">&#x25BC;</div>
<div class="btn" id="bu">&#x21BA;</div>
<div class="btn" id="br">&#x25BA;</div>
<div class="btn" id="bdd">&#x23EC;</div></div>
<div id="over"><h2>GAME OVER</h2><p id="os">SCORE: 0</p><button onclick="init()">&#x25BA; PLAY</button></div>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
const NC=document.getElementById('nc'),nctx=NC.getContext('2d');
const COLS=10,ROWS=20;
const SZ=Math.min(Math.floor((window.innerHeight-155)/ROWS),Math.floor((window.innerWidth-16)/(COLS+3)));
const NSZ=Math.floor(SZ*0.7);
C.width=COLS*SZ;C.height=ROWS*SZ;NC.width=4*NSZ;NC.height=4*NSZ;
const SHP=[[[1,1,1,1]],[[1,1],[1,1]],[[0,1,0],[1,1,1]],[[0,1,1],[1,1,0]],[[1,1,0],[0,1,1]],[[1,0,0],[1,1,1]],[[0,0,1],[1,1,1]]];
const CLR=['#00e5ff','#ffee00','#cc44ff','#00ff66','#ff4444','#4466ff','#ff8800'];
let grid,pc,nx,score,lines,level,hi=0,tid,state;
function mkGrid(){return Array.from({length:ROWS},()=>Array(COLS).fill(0));}
function mkPc(t){const s=SHP[t];return{t,sh:s.map(r=>[...r]),x:Math.floor((COLS-s[0].length)/2),y:0};}
function rot(s){return s[0].map((_,c)=>s.map(r=>r[c])).map(r=>[...r].reverse());}
function ok(sh,x,y){
  for(let r=0;r<sh.length;r++)for(let c=0;c<sh[r].length;c++){
    if(!sh[r][c])continue;const nx2=x+c,ny=y+r;
    if(nx2<0||nx2>=COLS||ny>=ROWS)return false;
    if(ny>=0&&grid[ny][nx2])return false;
  }return true;
}
function lock(){
  pc.sh.forEach((r,ri)=>r.forEach((v,ci)=>{if(v&&pc.y+ri>=0)grid[pc.y+ri][pc.x+ci]=pc.t+1;}));
  let cl=0;for(let r=ROWS-1;r>=0;){if(grid[r].every(c=>c)){grid.splice(r,1);grid.unshift(Array(COLS).fill(0));cl++;}else r--;}
  score+=[0,100,300,500,800][cl]*(level+1);lines+=cl;level=Math.floor(lines/10);
  if(score>hi)hi=score;
  document.getElementById('sc').textContent='SCORE: '+score;
  document.getElementById('lv').textContent=level;
  document.getElementById('ln').textContent=lines;
  clearInterval(tid);tid=setInterval(tick,Math.max(80,500-level*40));
  pc=nx;nx=mkPc(Math.floor(Math.random()*7));
  if(!ok(pc.sh,pc.x,pc.y)){state='over';clearInterval(tid);
    document.getElementById('os').textContent='SCORE: '+score+(score>0&&score==hi?' \u2605 HI':'');
    document.getElementById('over').style.display='block';}
  else draw();
}
function tick(){if(!ok(pc.sh,pc.x,pc.y+1))lock();else{pc.y++;draw();}}
function init(){
  grid=mkGrid();score=0;lines=0;level=0;state='play';
  pc=mkPc(Math.floor(Math.random()*7));nx=mkPc(Math.floor(Math.random()*7));
  document.getElementById('over').style.display='none';
  document.getElementById('sc').textContent='SCORE: 0';
  document.getElementById('lv').textContent='0';
  document.getElementById('ln').textContent='0';
  clearInterval(tid);tid=setInterval(tick,500);draw();
}
function dcell(c,ctx2,x,y,sz){
  ctx2.fillStyle=CLR[c-1];ctx2.fillRect(x*sz+1,y*sz+1,sz-2,sz-2);
  ctx2.fillStyle='rgba(255,255,255,.28)';ctx2.fillRect(x*sz+1,y*sz+1,sz-2,Math.min(5,sz/4));
  ctx2.fillStyle='rgba(0,0,0,.25)';ctx2.fillRect(x*sz+1,y*sz+sz-Math.min(4,sz/5)-1,sz-2,Math.min(4,sz/5));
}
function draw(){
  ctx.fillStyle='#000';ctx.fillRect(0,0,C.width,C.height);
  ctx.strokeStyle='rgba(255,255,255,.04)';ctx.lineWidth=.5;
  for(let x=0;x<=COLS;x++){ctx.beginPath();ctx.moveTo(x*SZ,0);ctx.lineTo(x*SZ,C.height);ctx.stroke();}
  for(let y=0;y<=ROWS;y++){ctx.beginPath();ctx.moveTo(0,y*SZ);ctx.lineTo(C.width,y*SZ);ctx.stroke();}
  for(let r=0;r<ROWS;r++)for(let c=0;c<COLS;c++)if(grid[r][c])dcell(grid[r][c],ctx,c,r,SZ);
  let gy=pc.y;while(ok(pc.sh,pc.x,gy+1))gy++;
  if(gy>pc.y)pc.sh.forEach((r,ri)=>r.forEach((v,ci)=>{
    if(v){ctx.fillStyle='rgba(255,255,255,.09)';ctx.fillRect((pc.x+ci)*SZ+1,(gy+ri)*SZ+1,SZ-2,SZ-2);}
  }));
  pc.sh.forEach((r,ri)=>r.forEach((v,ci)=>{if(v)dcell(pc.t+1,ctx,pc.x+ci,pc.y+ri,SZ);}));
  nctx.fillStyle='#000';nctx.fillRect(0,0,NC.width,NC.height);
  const ns=nx.sh,ox=Math.floor((4-ns[0].length)/2),oy=Math.floor((4-ns.length)/2);
  ns.forEach((r,ri)=>r.forEach((v,ci)=>{if(v)dcell(nx.t+1,nctx,ox+ci,oy+ri,NSZ);}));
}
function move(dx){if(ok(pc.sh,pc.x+dx,pc.y)){pc.x+=dx;draw();}}
function rotate(){const ns=rot(pc.sh);for(const dx of[0,-1,1,-2,2]){if(ok(ns,pc.x+dx,pc.y)){pc.sh=ns;pc.x+=dx;draw();return;}}}
function hardDrop(){while(ok(pc.sh,pc.x,pc.y+1))pc.y++;lock();}
document.addEventListener('keydown',e=>{
  if(state!='play')return;
  if(e.key=='ArrowLeft')move(-1);else if(e.key=='ArrowRight')move(1);
  else if(e.key=='ArrowDown'){tick();}else if(e.key=='ArrowUp'||e.key=='z'||e.key=='x')rotate();
  else if(e.key==' ')hardDrop();
  e.preventDefault();
});
document.getElementById('bl').addEventListener('click',()=>{if(state=='play')move(-1);});
document.getElementById('br').addEventListener('click',()=>{if(state=='play')move(1);});
document.getElementById('bd').addEventListener('click',()=>{if(state=='play')tick();});
document.getElementById('bu').addEventListener('click',()=>{if(state=='play')rotate();});
document.getElementById('bdd').addEventListener('click',()=>{if(state=='play')hardDrop();});
let tx=0,ty=0;
C.addEventListener('touchstart',e=>{tx=e.touches[0].clientX;ty=e.touches[0].clientY;},{passive:true});
C.addEventListener('touchend',e=>{
  if(state!='play')return;
  const dx=e.changedTouches[0].clientX-tx,dy=e.changedTouches[0].clientY-ty;
  if(Math.abs(dx)<15&&Math.abs(dy)<15)rotate();
  else if(Math.abs(dx)>Math.abs(dy)){if(dx>20)move(1);else if(dx<-20)move(-1);}
  else if(dy>20)tick();
},{passive:true});
init();
</script></body></html>
)EOF";

// ── Stellar Dodge ──────────────────────────────────────────────────────────────
static const char DODGE_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no"><title>STELLAR DODGE · COSMIC-CYD</title><style>*{margin:0;padding:0;box-sizing:border-box}body{background:#000;color:#c77dff;font-family:monospace;display:flex;flex-direction:column;align-items:center;height:100vh;overflow:hidden;gap:3px}.nav{background:#050011;border-bottom:1px solid #8338ec;padding:6px 12px;width:100%;display:flex;align-items:center;justify-content:space-between;font-size:12px}.nav a{color:#c77dff;text-decoration:none}canvas{display:block;border:1px solid rgba(131,56,236,.5);box-shadow:0 0 12px rgba(131,56,236,.35)}#dpad{display:flex;gap:12px;margin:5px 0}.btn{background:#05001a;border:1px solid #8338ec;color:#c77dff;font-size:22px;width:90px;height:52px;display:flex;align-items:center;justify-content:center;border-radius:6px;user-select:none;touch-action:manipulation;cursor:pointer}.btn:active{background:#8338ec;color:#fff}#over{display:none;position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);background:rgba(5,0,17,.97);border:1px solid #8338ec;padding:22px 28px;text-align:center;z-index:10;box-shadow:0 0 20px #8338ec}#over h2{color:#c77dff;letter-spacing:4px;margin-bottom:10px}#over p{color:#9055bb;margin-bottom:18px;font-size:13px;letter-spacing:2px}#over button{background:#000;border:1px solid #8338ec;color:#c77dff;padding:10px 24px;font-family:monospace;font-size:14px;cursor:pointer;letter-spacing:3px}#over button:hover{background:#8338ec;color:#fff}</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span id="sc">SCORE: 0</span><span>STELLAR DODGE</span></div>
<canvas id="c"></canvas>
<div id="dpad"><div class="btn" id="bl">&#x25C4; LEFT</div><div class="btn" id="br">RIGHT &#x25BA;</div></div>
<div id="over"><h2>&#x1F4A5; DESTROYED</h2><p id="os">SCORE: 0</p><button onclick="init()">&#x25BA; PLAY AGAIN</button></div>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
const W=Math.min(320,window.innerWidth-4);
const H=Math.min(window.innerHeight-115,400);
C.width=W;C.height=H;
const SW=26,SH=20;
let ship,objs,stars,score,hi=0,raf,state,speed,spawnT,spawnI,flamePh=0;
function mkShip(){return{x:W/2,y:H-28};}
function mkStar(){return{x:Math.random()*W,y:Math.random()*H,r:Math.random()*1.1+0.3,b:Math.random()};}
function mkObj(){
  const t=Math.floor(Math.random()*3);
  const r=t===0?Math.random()*11+7:t===1?Math.random()*5+4:Math.random()*9+5;
  return{x:r+Math.random()*(W-r*2),y:-r*2,r,
    dx:(Math.random()-.5)*(t===1?2.5:0.8)*speed,
    dy:(Math.random()*.5+0.8)*speed*(t===1?1.5:1),
    t,rot:0,rs:(Math.random()-.5)*0.07};
}
function init(){
  ship=mkShip();objs=[];stars=Array.from({length:55},mkStar);
  score=0;speed=1.6;spawnT=0;spawnI=75;state='play';
  document.getElementById('over').style.display='none';
  document.getElementById('sc').textContent='SCORE: 0';
  cancelAnimationFrame(raf);loop();
}
function drawShip(x,y){
  ctx.save();ctx.translate(x,y);
  flamePh+=0.3;
  ctx.shadowColor='#ff6600';ctx.shadowBlur=10;
  ctx.fillStyle='#ff4400';
  ctx.beginPath();ctx.moveTo(-SW/4,SH/2);ctx.lineTo(SW/4,SH/2);
  ctx.lineTo(0,SH/2+5+Math.sin(flamePh)*4);ctx.closePath();ctx.fill();
  ctx.shadowColor='#8338ec';ctx.shadowBlur=16;
  ctx.fillStyle='#c77dff';
  ctx.beginPath();ctx.moveTo(0,-SH/2);ctx.lineTo(SW/2,SH/2);ctx.lineTo(-SW/2,SH/2);ctx.closePath();ctx.fill();
  ctx.fillStyle='#00e5ff';ctx.shadowColor='#00e5ff';ctx.shadowBlur=8;
  ctx.beginPath();ctx.arc(0,-4,4,0,Math.PI*2);ctx.fill();
  ctx.restore();
}
function drawObj(o){
  ctx.save();ctx.translate(o.x,o.y);ctx.rotate(o.rot);
  if(o.t===0){
    ctx.shadowColor='#ff4400';ctx.shadowBlur=8;ctx.fillStyle='#cc4411';
    ctx.beginPath();
    for(let i=0;i<9;i++){const a=i/9*Math.PI*2,rr=o.r*(0.72+Math.sin(i*2.7)*0.28);i===0?ctx.moveTo(Math.cos(a)*rr,Math.sin(a)*rr):ctx.lineTo(Math.cos(a)*rr,Math.sin(a)*rr);}
    ctx.closePath();ctx.fill();ctx.strokeStyle='#ff7733';ctx.lineWidth=1;ctx.stroke();
  } else if(o.t===1){
    ctx.shadowColor='#44aaff';ctx.shadowBlur=16;
    const g=ctx.createLinearGradient(0,-o.r*5,0,o.r);
    g.addColorStop(0,'rgba(68,170,255,0)');g.addColorStop(1,'rgba(100,200,255,.85)');
    ctx.fillStyle=g;
    ctx.beginPath();ctx.moveTo(-o.r*.35,-o.r*5);ctx.lineTo(o.r*.35,-o.r*5);ctx.lineTo(o.r,o.r);ctx.lineTo(-o.r,o.r);ctx.closePath();ctx.fill();
    ctx.fillStyle='#eef';ctx.beginPath();ctx.arc(0,0,o.r,0,Math.PI*2);ctx.fill();
  } else {
    ctx.shadowColor='#aaa';ctx.shadowBlur=4;ctx.fillStyle='#666';
    ctx.fillRect(-o.r,-o.r*.55,o.r*2,o.r*1.1);
    ctx.strokeStyle='#bbb';ctx.lineWidth=1;ctx.strokeRect(-o.r,-o.r*.55,o.r*2,o.r*1.1);
    ctx.strokeStyle='#888';ctx.lineWidth=.5;
    ctx.beginPath();ctx.moveTo(0,-o.r*.55);ctx.lineTo(0,o.r*.55);ctx.stroke();
  }
  ctx.restore();
}
function hits(s,o){const dx=s.x-o.x,dy=s.y-o.y,hr=o.r+9;return dx*dx+dy*dy<hr*hr;}
function loop(){
  if(state!=='play')return;
  raf=requestAnimationFrame(loop);
  ctx.fillStyle='#00000a';ctx.fillRect(0,0,W,H);
  stars.forEach(s=>{s.y+=0.25+s.b*0.35;if(s.y>H){s.y=0;s.x=Math.random()*W;}ctx.fillStyle='rgba(255,255,255,'+(0.25+s.b*.55)+')';ctx.beginPath();ctx.arc(s.x,s.y,s.r,0,Math.PI*2);ctx.fill();});
  speed=1.6+score/900;
  spawnT++;if(spawnT>=spawnI){spawnT=0;objs.push(mkObj());if(spawnI>28)spawnI-=.4;}
  if(keys.l)ship.x-=3.5;if(keys.r)ship.x+=3.5;
  ship.x=Math.max(SW/2,Math.min(W-SW/2,ship.x));
  objs=objs.filter(o=>{
    o.x+=o.dx;o.y+=o.dy;o.rot+=o.rs;
    if(o.y>H+30)return false;
    drawObj(o);
    if(hits(ship,o)){state='dead';cancelAnimationFrame(raf);
      document.getElementById('os').textContent='SCORE: '+score+(score>=hi&&score>0?' \u2605 NEW HI!':'   HI: '+hi);
      if(score>hi)hi=score;
      document.getElementById('over').style.display='block';return false;}
    return true;
  });
  drawShip(ship.x,ship.y);
  score++;
  document.getElementById('sc').textContent='SCORE: '+score+(hi>0?'  HI: '+hi:'');
}
const keys={l:false,r:false};
document.addEventListener('keydown',e=>{if(e.key==='ArrowLeft')keys.l=true;if(e.key==='ArrowRight')keys.r=true;e.preventDefault();});
document.addEventListener('keyup',e=>{if(e.key==='ArrowLeft')keys.l=false;if(e.key==='ArrowRight')keys.r=false;});
function hold(id,k){
  const el=document.getElementById(id);
  el.addEventListener('pointerdown',e=>{keys[k]=true;e.preventDefault();},{passive:false});
  el.addEventListener('pointerup',()=>keys[k]=false);
  el.addEventListener('pointerleave',()=>keys[k]=false);
}
hold('bl','l');hold('br','r');
init();
</script></body></html>
)EOF";

// ── Asteroids ─────────────────────────────────────────────────────────────────
static const char ASTEROIDS_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no"><title>ASTEROIDS · COSMIC-CYD</title><style>*{margin:0;padding:0;box-sizing:border-box}body{background:#000;color:#c77dff;font-family:monospace;display:flex;flex-direction:column;align-items:center;height:100vh;overflow:hidden;gap:2px}.nav{background:#050011;border-bottom:1px solid #8338ec;padding:6px 12px;width:100%;display:flex;align-items:center;justify-content:space-between;font-size:12px}.nav a{color:#c77dff;text-decoration:none}canvas{display:block;border:1px solid rgba(131,56,236,.5);box-shadow:0 0 12px rgba(131,56,236,.35)}#ctrls{display:flex;gap:10px;margin:4px 0;align-items:flex-end;justify-content:center}#lp{display:flex;flex-direction:column;gap:4px}#rp{display:flex;flex-direction:column;align-items:center;gap:4px}#trow{display:flex;gap:4px}.btn{background:#05001a;border:1px solid #8338ec;color:#c77dff;font-size:12px;font-family:monospace;letter-spacing:1px;width:78px;height:46px;display:flex;align-items:center;justify-content:center;border-radius:6px;user-select:none;touch-action:manipulation;cursor:pointer;text-align:center;line-height:1.3}.btn:active{background:#8338ec;color:#fff}#bF{border-color:#ff4488;color:#ff88bb}#bF:active{background:#aa1144;color:#fff}#bW{border-color:#00e5ff;color:#44ddff}#bW:active{background:#005566;color:#fff}#bT{width:160px}#over{display:none;position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);background:rgba(5,0,17,.97);border:1px solid #8338ec;padding:22px 28px;text-align:center;z-index:10;box-shadow:0 0 20px #8338ec}#over h2{color:#c77dff;letter-spacing:4px;margin-bottom:10px}#over p{color:#9055bb;margin-bottom:18px;font-size:13px;letter-spacing:2px}#over button{background:#000;border:1px solid #8338ec;color:#c77dff;padding:10px 24px;font-family:monospace;font-size:14px;cursor:pointer;letter-spacing:3px}#over button:hover{background:#8338ec;color:#fff}</style></head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span id="sc">SCORE: 0</span><span>ASTEROIDS</span></div>
<canvas id="c"></canvas>
<div id="ctrls">
<div id="lp"><div class="btn" id="bF">&#x1F525;<br>FIRE</div><div class="btn" id="bW">&#x26A1;<br>WARP</div></div>
<div id="rp"><div id="trow"><div class="btn" id="bL">&#x21BA; TURN</div><div class="btn" id="bR">TURN &#x21BB;</div></div><div class="btn" id="bT">&#x25B2; THRUST</div></div>
</div>
<div id="over"><h2>&#x1F4A5; DESTROYED</h2><p id="os">SCORE: 0</p><button onclick="init()">&#x25BA; PLAY AGAIN</button></div>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
const W=Math.min(340,window.innerWidth-4),H=Math.min(window.innerHeight-158,370);
C.width=W;C.height=H;
const TSPDV=0.065,THR=0.14,DRAG=0.988,BSPD=7,BLIFE=52,FCD=12;
let ship,bullets,asts,parts,score,hi=0,lives,state,fcd,level,raf;
function mkShip(){return{x:W/2,y:H/2,vx:0,vy:0,ang:0,inv:180};}
function mkPoly(r,n){const p=[];for(let i=0;i<n;i++){const a=i/n*Math.PI*2,rr=r*(.72+Math.random()*.32);p.push([Math.cos(a)*rr,Math.sin(a)*rr]);}return p;}
function mkAst(x,y,sz){
  const a=Math.random()*Math.PI*2,spd=(.9+level*.12)*(1+(3-sz)*.4);
  const r=sz===3?27:sz===2?15:8;
  return{x:x??Math.random()*W,y:y??Math.random()*H,vx:Math.cos(a)*spd,vy:Math.sin(a)*spd,sz,r,rot:0,rs:(Math.random()-.5)*.025,poly:mkPoly(r,9+Math.floor(Math.random()*4))};
}
function spawnWave(){
  asts=[];
  for(let i=0;i<3+level;i++){
    let x,y;do{x=Math.random()*W;y=Math.random()*H;}while(Math.hypot(x-W/2,y-H/2)<80);
    asts.push(mkAst(x,y,3));
  }
}
function init(){
  ship=mkShip();bullets=[];parts=[];score=0;lives=3;level=1;fcd=0;state='play';
  spawnWave();
  document.getElementById('over').style.display='none';
  document.getElementById('sc').textContent='SCORE: 0';
  cancelAnimationFrame(raf);loop();
}
function wrap(o){if(o.x<0)o.x+=W;if(o.x>W)o.x-=W;if(o.y<0)o.y+=H;if(o.y>H)o.y-=H;}
function fire(){
  if(fcd>0||state!=='play')return;fcd=FCD;
  bullets.push({x:ship.x+Math.sin(ship.ang)*13,y:ship.y-Math.cos(ship.ang)*13,vx:ship.vx+Math.sin(ship.ang)*BSPD,vy:ship.vy-Math.cos(ship.ang)*BSPD,life:BLIFE});
}
function warp(){
  if(state!=='play')return;
  const ox=ship.x,oy=ship.y;
  ship.x=50+Math.random()*(W-100);ship.y=50+Math.random()*(H-100);
  ship.vx=0;ship.vy=0;ship.inv=120;
  for(let i=0;i<14;i++)parts.push({x:ox,y:oy,vx:(Math.random()-.5)*4,vy:(Math.random()-.5)*4,life:35,col:'#00e5ff'});
  for(let i=0;i<8;i++)parts.push({x:ship.x,y:ship.y,vx:(Math.random()-.5)*3,vy:(Math.random()-.5)*3,life:25,col:'#44ffcc'});
}
function blowAst(i){
  const a=asts[i];
  const cols=['','#ffddaa','#ffaa44','#ff6622'];
  for(let k=0;k<8+a.sz*2;k++)parts.push({x:a.x,y:a.y,vx:(Math.random()-.5)*4,vy:(Math.random()-.5)*4,life:25+Math.random()*20,col:cols[a.sz]});
  score+=[0,100,50,20][a.sz];if(score>hi)hi=score;
  document.getElementById('sc').textContent='SCORE: '+score+'  HI: '+hi;
  const kids=[];
  if(a.sz>1){kids.push(mkAst(a.x,a.y,a.sz-1));kids.push(mkAst(a.x,a.y,a.sz-1));}
  asts.splice(i,1,...kids);
  if(asts.length===0){level++;spawnWave();}
}
function die(){
  for(let k=0;k<20;k++)parts.push({x:ship.x,y:ship.y,vx:(Math.random()-.5)*5,vy:(Math.random()-.5)*5,life:40,col:'#c77dff'});
  lives--;
  if(lives<=0){
    state='over';cancelAnimationFrame(raf);
    document.getElementById('os').textContent='SCORE: '+score+(score>0&&score===hi?' \u2605 NEW HI!':'  HI: '+hi);
    document.getElementById('over').style.display='block';return;
  }
  ship=mkShip();
}
function drawShip(){
  if(ship.inv>0&&Math.floor(ship.inv/5)%2)return;
  ctx.save();ctx.translate(ship.x,ship.y);ctx.rotate(ship.ang);
  ctx.shadowColor='#8338ec';ctx.shadowBlur=10;ctx.strokeStyle='#c77dff';ctx.lineWidth=1.5;
  ctx.beginPath();ctx.moveTo(0,-12);ctx.lineTo(8,10);ctx.lineTo(0,6);ctx.lineTo(-8,10);ctx.closePath();ctx.stroke();
  if(keys.t){
    ctx.strokeStyle='#ff6600';ctx.shadowColor='#ff4400';ctx.shadowBlur=14;
    ctx.beginPath();ctx.moveTo(-4,8);ctx.lineTo(0,16+Math.random()*7);ctx.lineTo(4,8);ctx.stroke();
  }
  ctx.restore();
}
function loop(){
  if(state!=='play')return;
  raf=requestAnimationFrame(loop);
  ctx.fillStyle='rgba(0,0,8,.88)';ctx.fillRect(0,0,W,H);
  ctx.fillStyle='rgba(199,125,255,.7)';ctx.font='11px monospace';
  ctx.fillText('\u2665 '.repeat(lives),4,12);
  ctx.fillText('LVL '+level,W-42,12);
  if(keys.l)ship.ang-=TSPDV;
  if(keys.r)ship.ang+=TSPDV;
  if(keys.t){ship.vx+=Math.sin(ship.ang)*THR;ship.vy-=Math.cos(ship.ang)*THR;}
  ship.vx*=DRAG;ship.vy*=DRAG;
  ship.x+=ship.vx;ship.y+=ship.vy;wrap(ship);
  if(ship.inv>0)ship.inv--;
  if(fcd>0)fcd--;
  if(keys.f)fire();
  bullets=bullets.filter(b=>{
    b.x+=b.vx;b.y+=b.vy;b.life--;wrap(b);
    if(b.life<=0)return false;
    ctx.fillStyle='#fff';ctx.shadowColor='#00e5ff';ctx.shadowBlur=5;
    ctx.beginPath();ctx.arc(b.x,b.y,2,0,Math.PI*2);ctx.fill();
    for(let i=asts.length-1;i>=0;i--){
      if(Math.hypot(b.x-asts[i].x,b.y-asts[i].y)<asts[i].r){blowAst(i);return false;}
    }
    return true;
  });
  asts.forEach((a,i)=>{
    a.x+=a.vx;a.y+=a.vy;a.rot+=a.rs;wrap(a);
    ctx.save();ctx.translate(a.x,a.y);ctx.rotate(a.rot);
    const col=a.sz===3?'#ff6622':a.sz===2?'#ffaa44':'#ffddaa';
    ctx.strokeStyle=col;ctx.shadowColor=col;ctx.shadowBlur=5;ctx.lineWidth=1.5;
    ctx.beginPath();a.poly.forEach((p,j)=>j===0?ctx.moveTo(p[0],p[1]):ctx.lineTo(p[0],p[1]));
    ctx.closePath();ctx.stroke();ctx.restore();
    if(ship.inv<=0&&Math.hypot(ship.x-a.x,ship.y-a.y)<a.r-2)die();
  });
  ctx.shadowBlur=0;
  parts=parts.filter(p=>{
    p.x+=p.vx;p.y+=p.vy;p.vx*=.93;p.vy*=.93;p.life--;
    ctx.globalAlpha=Math.max(0,p.life/40);ctx.fillStyle=p.col;
    ctx.beginPath();ctx.arc(p.x,p.y,1.5,0,Math.PI*2);ctx.fill();
    return p.life>0;
  });
  ctx.globalAlpha=1;
  drawShip();
}
const keys={l:false,r:false,t:false,f:false};
document.addEventListener('keydown',e=>{
  if(e.key==='ArrowLeft')keys.l=true;if(e.key==='ArrowRight')keys.r=true;
  if(e.key==='ArrowUp')keys.t=true;if(e.key===' '||e.key==='z')keys.f=true;
  if(e.key==='x')warp();e.preventDefault();
});
document.addEventListener('keyup',e=>{
  if(e.key==='ArrowLeft')keys.l=false;if(e.key==='ArrowRight')keys.r=false;
  if(e.key==='ArrowUp')keys.t=false;if(e.key===' '||e.key==='z')keys.f=false;
});
function hold(id,k){
  const el=document.getElementById(id);
  el.addEventListener('pointerdown',e=>{keys[k]=true;e.preventDefault();},{passive:false});
  el.addEventListener('pointerup',()=>keys[k]=false);
  el.addEventListener('pointerleave',()=>keys[k]=false);
}
hold('bL','l');hold('bT','t');hold('bR','r');hold('bF','f');
document.getElementById('bW').addEventListener('pointerdown',e=>{warp();e.preventDefault();},{passive:false});
init();
</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char FIREWORKS_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>FIREWORKS · COSMIC-S3</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);
  border-bottom:1px solid #ffaa00;padding:5px 10px;display:flex;
  justify-content:space-between;font-size:11px;z-index:9}
.nav a{{color:#ffaa00;text-decoration:none}}
canvas{display:block}
</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>FIREWORKS</span></div>
<canvas id="c"></canvas>
<script>
const C=document.getElementById('c');
const ctx=C.getContext('2d');
let W,H;
function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();init&&init();});
resize();

const rockets=[];
const sparks=[];
function launch(){
  const x=W*0.15+Math.random()*W*0.7;
  rockets.push({x,y:H,tx:W*0.1+Math.random()*W*0.8,ty:H*0.1+Math.random()*H*0.4,trail:[]});
}
function burst(x,y){
  const hue=Math.random()*360;
  const n=60+Math.floor(Math.random()*40);
  for(let i=0;i<n;i++){
    const a=Math.random()*Math.PI*2,s=2+Math.random()*5;
    sparks.push({x,y,vx:Math.cos(a)*s,vy:Math.sin(a)*s,life:1,
      r:Math.round(Math.sin((hue/360)*Math.PI*2)*127+128),
      g:Math.round(Math.sin((hue/360)*Math.PI*2+2.09)*127+128),
      b:Math.round(Math.sin((hue/360)*Math.PI*2+4.19)*127+128)});
  }
}
setInterval(launch,700+Math.random()*500);
function draw(){
  ctx.fillStyle='rgba(0,0,0,0.18)';ctx.fillRect(0,0,W,H);
  for(let i=rockets.length-1;i>=0;i--){
    const r=rockets[i];
    const dx=r.tx-r.x,dy=r.ty-r.y,d=Math.sqrt(dx*dx+dy*dy);
    r.trail.push([r.x,r.y]);if(r.trail.length>12)r.trail.shift();
    if(d<6){burst(r.x,r.y);rockets.splice(i,1);continue;}
    const sp=4+Math.random()*2;r.x+=dx/d*sp;r.y+=dy/d*sp;
    r.trail.forEach(([tx,ty],j)=>{
      ctx.fillStyle=`rgba(255,200,80,${j/r.trail.length*0.8})`;
      ctx.beginPath();ctx.arc(tx,ty,1.5,0,Math.PI*2);ctx.fill();
    });
    ctx.fillStyle='#ffe080';ctx.beginPath();ctx.arc(r.x,r.y,2,0,Math.PI*2);ctx.fill();
  }
  for(let i=sparks.length-1;i>=0;i--){
    const s=sparks[i];
    s.x+=s.vx;s.y+=s.vy;s.vy+=0.08;s.vx*=0.98;s.life-=0.016;
    if(s.life<=0){sparks.splice(i,1);continue;}
    ctx.fillStyle=`rgba(${s.r},${s.g},${s.b},${s.life})`;
    ctx.beginPath();ctx.arc(s.x,s.y,1.5*s.life,0,Math.PI*2);ctx.fill();
  }
  requestAnimationFrame(draw);
}
draw();

</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char CORAL_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>CORAL REEF · COSMIC-S3</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);
  border-bottom:1px solid #ff6688;padding:5px 10px;display:flex;
  justify-content:space-between;font-size:11px;z-index:9}
.nav a{{color:#ff6688;text-decoration:none}}
canvas{display:block}
</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>CORAL REEF</span></div>
<canvas id="c"></canvas>
<script>
const C=document.getElementById('c');
const ctx=C.getContext('2d');
let W,H;
function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();init&&init();});
resize();

const COLS=60,ROWS=45;
const grid=Array.from({length:COLS},()=>new Uint8Array(ROWS));
const age=Array.from({length:COLS},()=>new Float32Array(ROWS));
let gen=0;
// seed bottom row
for(let x=0;x<COLS;x++)if(Math.random()<0.35){grid[x][ROWS-1]=1;age[x][ROWS-1]=Math.random();}
function step(){
  const ng=Array.from({length:COLS},(_,x)=>new Uint8Array(grid[x]));
  for(let x=1;x<COLS-1;x++)for(let y=1;y<ROWS-1;y++){
    if(!grid[x][y]){
      let n=0;
      for(let dx=-1;dx<=1;dx++)for(let dy=-1;dy<=1;dy++){if(dx==0&&dy==0)continue;if(grid[x+dx][y+dy])n++;}
      if(n>=1&&n<=3&&grid[x][y+1]&&Math.random()<0.18)ng[x][y]=1;
    }
  }
  for(let x=0;x<COLS;x++)for(let y=0;y<ROWS;y++){grid[x][y]=ng[x][y];if(grid[x][y])age[x][y]+=0.002;}
  gen++;if(gen>300){gen=0;for(let x=0;x<COLS;x++)for(let y=0;y<ROWS;y++){grid[x][y]=0;age[x][y]=0;}
    for(let x=0;x<COLS;x++)if(Math.random()<0.35){grid[x][ROWS-1]=1;age[x][ROWS-1]=Math.random();}}
}
let t=0;
function draw(){
  t+=0.02;
  ctx.fillStyle='#000c1a';ctx.fillRect(0,0,W,H);
  const cw=W/COLS,ch=H/ROWS;
  for(let x=0;x<COLS;x++)for(let y=0;y<ROWS;y++){
    if(grid[x][y]){
      const d=(ROWS-y)/ROWS,a=age[x][y];
      const r=Math.round(220-d*100+Math.sin(t+x*0.3)*20);
      const g=Math.round(80+d*120+Math.sin(t*0.7+y*0.2)*15);
      const b=Math.round(120+d*80);
      ctx.fillStyle=`rgb(${r},${g},${b})`;
      ctx.fillRect(x*cw,y*ch,cw+1,ch+1);
    }
  }
  // water shimmer
  for(let bx=0;bx<W;bx+=40){
    const sy=H*0.05+Math.sin(t+bx*0.04)*8;
    ctx.strokeStyle='rgba(100,180,255,0.12)';ctx.lineWidth=2;
    ctx.beginPath();ctx.moveTo(bx,sy);ctx.lineTo(bx+40,sy+Math.sin(t+bx*0.05)*6);ctx.stroke();
  }
  if(t%0.5<0.02)step();
  requestAnimationFrame(draw);
}
draw();setInterval(step,80);

</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char CWAVES_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>C-WAVES · COSMIC-S3</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);
  border-bottom:1px solid #44aaff;padding:5px 10px;display:flex;
  justify-content:space-between;font-size:11px;z-index:9}
.nav a{{color:#44aaff;text-decoration:none}}
canvas{display:block}
</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>C-WAVES</span></div>
<canvas id="c"></canvas>
<script>
const C=document.getElementById('c');
const ctx=C.getContext('2d');
let W,H;
function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();init&&init();});
resize();

let t=0;
function draw(){
  t+=0.025;
  ctx.fillStyle='rgba(0,0,18,0.25)';ctx.fillRect(0,0,W,H);
  const cx=W/2,cy=H/2;
  for(let w=0;w<6;w++){
    const off=w*Math.PI/3+t*0.25;
    const amp=15+w*10,rad=30+w*20;
    const hue=(w*55+t*20)%360;
    ctx.strokeStyle=`hsla(${hue},100%,65%,0.8)`;ctx.lineWidth=2;
    ctx.beginPath();let first=true;
    for(let a=30;a<=330;a+=3){
      const ra=a*Math.PI/180+off;
      const wh=Math.sin(ra*3+t*4)*amp*0.3+Math.sin(ra*2+t*2)*amp*0.2;
      const cr=rad+wh;
      const x=cx+Math.cos(ra)*cr,y=cy+Math.sin(ra)*cr;
      first?(ctx.moveTo(x,y),first=false):ctx.lineTo(x,y);
    }
    ctx.stroke();
    // glow endpoints
    const s=30*Math.PI/180+off,e=330*Math.PI/180+off;
    ctx.fillStyle=`hsla(${hue},100%,90%,0.9)`;
    ctx.beginPath();ctx.arc(cx+Math.cos(s)*rad,cy+Math.sin(s)*rad,4,0,Math.PI*2);ctx.fill();
    ctx.beginPath();ctx.arc(cx+Math.cos(e)*rad,cy+Math.sin(e)*rad,4,0,Math.PI*2);ctx.fill();
  }
  requestAnimationFrame(draw);
}
draw();

</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char DEEPSTARS_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>DEEP STARS · COSMIC-S3</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);
  border-bottom:1px solid #aaaaff;padding:5px 10px;display:flex;
  justify-content:space-between;font-size:11px;z-index:9}
.nav a{{color:#aaaaff;text-decoration:none}}
canvas{display:block}
</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>DEEP STARS</span></div>
<canvas id="c"></canvas>
<script>
const C=document.getElementById('c');
const ctx=C.getContext('2d');
let W,H;
function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();init&&init();});
resize();

const N=150;
const sx=new Float32Array(N),sy=new Float32Array(N),sz=new Float32Array(N),sbr=new Float32Array(N);
for(let i=0;i<N;i++){sx[i]=(Math.random()-0.5)*400;sy[i]=(Math.random()-0.5)*300;sz[i]=Math.random()*100+1;sbr[i]=80+Math.random()*175;}
let t=0;
function draw(){
  t+=0.016;
  ctx.fillStyle='#000007';ctx.fillRect(0,0,W,H);
  // nebula wisps
  for(let nx=0;nx<W;nx+=20){for(let ny=0;ny<H;ny+=20){
    const nd=Math.sin((nx+t*8)*0.018)*Math.cos((ny+t*6)*0.022);
    if(nd>0.68){const ni=(nd-0.68)*350|0;ctx.fillStyle=`rgba(${ni>>2},0,${ni},0.4)`;ctx.fillRect(nx,ny,20,20);}
  }}
  const cx=W/2,cy=H/2;
  for(let i=0;i<N;i++){
    sz[i]-=0.4+Math.sin(t*0.08)*0.25;
    if(sz[i]<1){sx[i]=(Math.random()-0.5)*400;sy[i]=(Math.random()-0.5)*300;sz[i]=90+Math.random()*10;sbr[i]=80+Math.random()*175;}
    const px=cx+sx[i]/sz[i]*120,py=cy+sy[i]/sz[i]*120;
    if(px<0||px>W||py<0||py>H)continue;
    const size=Math.max(1,(100/sz[i])*2.5)|0;
    const br=Math.min(255,(sbr[i]/sz[i]*22))|0;
    let color;
    if(br>200)color=`rgb(${br},${br},255)`;
    else if(br>140)color=`rgb(255,${br},${br})`;
    else if(br>90)color=`rgb(255,${br},${br>>1})`;
    else color=`rgb(${br},${br>>1},${br>>2})`;
    ctx.fillStyle=color;ctx.beginPath();ctx.arc(px,py,size,0,Math.PI*2);ctx.fill();
    if(sz[i]<18&&(t*10+i)%3<1){ctx.strokeStyle=`rgba(${br},${br},255,0.4)`;ctx.lineWidth=1;ctx.beginPath();ctx.arc(px,py,size+2,0,Math.PI*2);ctx.stroke();}
  }
  requestAnimationFrame(draw);
}
draw();

</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char FLOWFIELD_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>FLOW FIELD · COSMIC-S3</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);
  border-bottom:1px solid #ff8844;padding:5px 10px;display:flex;
  justify-content:space-between;font-size:11px;z-index:9}
.nav a{{color:#ff8844;text-decoration:none}}
canvas{display:block}
</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>FLOW FIELD</span></div>
<canvas id="c"></canvas>
<script>
const C=document.getElementById('c');
const ctx=C.getContext('2d');
let W,H;
function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();init&&init();});
resize();

const NP=120;
const px=new Float32Array(NP),py=new Float32Array(NP),pa=new Float32Array(NP);
for(let i=0;i<NP;i++){px[i]=Math.random()*800;py[i]=Math.random()*600;pa[i]=Math.random()*2;}
let t=0;
function init(){for(let i=0;i<NP;i++){px[i]=Math.random()*W;py[i]=Math.random()*H;pa[i]=Math.random()*2;}}
function draw(){
  t+=0.03;
  ctx.fillStyle='rgba(0,0,0,0.04)';ctx.fillRect(0,0,W,H);
  const sc=0.003;
  for(let i=0;i<NP;i++){
    const fx=px[i]*sc,fy=py[i]*sc;
    const angle=Math.sin(fx+t)*Math.cos(fy+t)*Math.PI;
    const strength=(Math.sin(fx*2+t*0.5)+1)*0.5;
    const vx=Math.cos(angle)*strength*2.2,vy=Math.sin(angle)*strength*2.2;
    const ox=px[i],oy=py[i];
    px[i]+=vx;py[i]+=vy;pa[i]+=0.007;
    if(px[i]<0||px[i]>W||py[i]<0||py[i]>H||pa[i]>2){px[i]=Math.random()*W;py[i]=Math.random()*H;pa[i]=0;continue;}
    const al=1-pa[i]/2;
    const hue=(angle/Math.PI*180+t*30)%360;
    ctx.strokeStyle=`hsla(${hue|0},100%,65%,${al*0.7})`;ctx.lineWidth=1.5;
    ctx.beginPath();ctx.moveTo(ox,oy);ctx.lineTo(px[i],py[i]);ctx.stroke();
  }
  requestAnimationFrame(draw);
}
draw();

</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char METABALLS_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>METABALLS · COSMIC-S3</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);
  border-bottom:1px solid #cc44ff;padding:5px 10px;display:flex;
  justify-content:space-between;font-size:11px;z-index:9}
.nav a{{color:#cc44ff;text-decoration:none}}
canvas{display:block}
</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>METABALLS</span></div>
<canvas id="c"></canvas>
<script>
const C=document.getElementById('c');
const ctx=C.getContext('2d');
let W,H;
function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();init&&init();});
resize();

const S=3;let IW,IH,ofc,octx;
function init(){IW=W/S|0;IH=H/S|0;ofc=new OffscreenCanvas(IW,IH);octx=ofc.getContext('2d');}
let t=0;
function draw(){
  t+=0.02;
  const id=octx.createImageData(IW,IH);const d=id.data;
  for(let y=0;y<IH;y++)for(let x=0;x<IW;x++){
    let f=0;
    for(let b=0;b<4;b++){
      const bx=IW/2+Math.cos(t+b*Math.PI/2)*IW*0.28;
      const by=IH/2+Math.sin(t*0.7+b*Math.PI/2)*IH*0.28;
      const dx=x-bx,dy=y-by,dist=Math.sqrt(dx*dx+dy*dy)||0.01;
      f+=900/(dist*dist);
    }
    if(f>1){
      const ff=Math.min(f,8);
      const i=(y*IW+x)*4;
      d[i]=Math.min(255,ff*40)|0;
      d[i+1]=Math.min(255,ff*20)|0;
      d[i+2]=Math.min(255,ff*70)|0;
      d[i+3]=255;
    }
  }
  octx.putImageData(id,0,0);
  ctx.fillStyle='#000';ctx.fillRect(0,0,W,H);
  ctx.drawImage(ofc,0,0,W,H);
  requestAnimationFrame(draw);
}
init();draw();

</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char GOOP_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>GOOP · COSMIC-S3</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);
  border-bottom:1px solid #88ff44;padding:5px 10px;display:flex;
  justify-content:space-between;font-size:11px;z-index:9}
.nav a{{color:#88ff44;text-decoration:none}}
canvas{display:block}
</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>GOOP</span></div>
<canvas id="c"></canvas>
<script>
const C=document.getElementById('c');
const ctx=C.getContext('2d');
let W,H;
function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();init&&init();});
resize();

let t=0;
function draw(){
  t+=0.018;
  ctx.fillStyle='rgba(0,0,0,0.15)';ctx.fillRect(0,0,W,H);
  const cx=W/2,cy=H/2;
  const blobs=[];
  for(let b=0;b<8;b++){
    const a=b*Math.PI/4+t*0.5;
    const r=Math.min(W,H)*0.22+Math.sin(t*2+b)*Math.min(W,H)*0.06;
    blobs.push({x:cx+Math.cos(a)*r,y:cy+Math.sin(a)*r,
      sz:Math.min(W,H)*0.05+Math.sin(t*3+b*0.7)*Math.min(W,H)*0.025,
      hue:(b*45+t*25)%360});
  }
  blobs.forEach((bl,i)=>{
    const g=ctx.createRadialGradient(bl.x,bl.y,0,bl.x,bl.y,bl.sz*1.6);
    g.addColorStop(0,`hsla(${bl.hue},100%,65%,0.9)`);
    g.addColorStop(1,'transparent');
    ctx.fillStyle=g;ctx.beginPath();ctx.arc(bl.x,bl.y,bl.sz*1.6,0,Math.PI*2);ctx.fill();
    if(i<7){
      const n=blobs[i+1];
      ctx.strokeStyle=`hsla(${bl.hue},100%,60%,0.4)`;ctx.lineWidth=3;
      ctx.beginPath();ctx.moveTo(bl.x,bl.y);
      ctx.quadraticCurveTo(cx+Math.sin(t)*30,cy+Math.cos(t)*30,n.x,n.y);ctx.stroke();
    }
  });
  requestAnimationFrame(draw);
}
draw();

</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char WORMHOLE_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>WORMHOLE · COSMIC-S3</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);
  border-bottom:1px solid #00ffcc;padding:5px 10px;display:flex;
  justify-content:space-between;font-size:11px;z-index:9}
.nav a{{color:#00ffcc;text-decoration:none}}
canvas{display:block}
</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>WORMHOLE</span></div>
<canvas id="c"></canvas>
<script>
const C=document.getElementById('c');
const ctx=C.getContext('2d');
let W,H;
function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();init&&init();});
resize();

let t=0;
function draw(){
  t+=0.025;
  ctx.fillStyle='rgba(0,0,0,0.2)';ctx.fillRect(0,0,W,H);
  const cx=W/2,cy=H/2;
  const N=28;
  for(let i=0;i<N;i++){
    const phase=t+i*(Math.PI*2/N)*2;
    const rad=Math.max(4,(1-i/N)*Math.min(W,H)*0.46);
    const hue=(i*13+t*40)%360;
    const alpha=0.55+Math.sin(phase)*0.25;
    ctx.strokeStyle=`hsla(${hue|0},100%,65%,${alpha})`;
    ctx.lineWidth=1.5+(i/N)*2;
    // slightly squished rotating ellipse
    ctx.save();ctx.translate(cx,cy);ctx.rotate(t*0.4+i*0.18);
    ctx.beginPath();ctx.ellipse(0,0,rad,rad*0.62,0,0,Math.PI*2);ctx.stroke();
    ctx.restore();
  }
  // center vortex glow
  const g=ctx.createRadialGradient(cx,cy,0,cx,cy,Math.min(W,H)*0.08);
  g.addColorStop(0,'rgba(0,255,200,0.7)');g.addColorStop(1,'transparent');
  ctx.fillStyle=g;ctx.beginPath();ctx.arc(cx,cy,Math.min(W,H)*0.08,0,Math.PI*2);ctx.fill();
  requestAnimationFrame(draw);
}
draw();

</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char CRYSTAL_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>CRYSTAL · COSMIC-S3</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);
  border-bottom:1px solid #aaddff;padding:5px 10px;display:flex;
  justify-content:space-between;font-size:11px;z-index:9}
.nav a{{color:#aaddff;text-decoration:none}}
canvas{display:block}
</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>CRYSTAL</span></div>
<canvas id="c"></canvas>
<script>
const C=document.getElementById('c');
const ctx=C.getContext('2d');
let W,H;
function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();init&&init();});
resize();

let t=0;
function draw(){
  t+=0.022;
  ctx.fillStyle='rgba(0,0,20,0.22)';ctx.fillRect(0,0,W,H);
  const cx=W/2,cy=H/2;
  for(let layer=0;layer<6;layer++){
    const la=t*0.3+layer*Math.PI/3;
    const lr=Math.min(W,H)*(0.06+layer*0.07);
    for(let k=0;k<6;k++){
      const a=la+k*Math.PI/3;
      const kx=cx+Math.cos(a)*lr,ky=cy+Math.sin(a)*lr;
      const sz=Math.min(W,H)*(0.025+Math.sin(t*2+layer+k)*0.012);
      const hue=(layer*60+k*20+t*30)%360;
      ctx.strokeStyle=`hsla(${hue|0},100%,75%,0.85)`;ctx.lineWidth=1.5;
      for(let s=0;s<6;s++){
        const sa=a+s*Math.PI/3;
        ctx.beginPath();ctx.moveTo(kx,ky);
        ctx.lineTo(kx+Math.cos(sa)*sz,ky+Math.sin(sa)*sz);ctx.stroke();
        ctx.beginPath();
        ctx.moveTo(kx+Math.cos(sa)*sz,ky+Math.sin(sa)*sz);
        ctx.lineTo(kx+Math.cos(sa+Math.PI/3)*sz,ky+Math.sin(sa+Math.PI/3)*sz);ctx.stroke();
      }
      ctx.fillStyle=`hsla(${hue|0},100%,95%,0.9)`;
      ctx.beginPath();ctx.arc(kx,ky,2,0,Math.PI*2);ctx.fill();
    }
  }
  // drifting sparkles
  for(let i=0;i<18;i++){
    const pt=t*1.4+i*0.4;
    const px=cx+Math.sin(pt)*Math.min(W,H)*0.32+Math.cos(pt*0.7)*Math.min(W,H)*0.15;
    const py=cy+Math.cos(pt)*Math.min(W,H)*0.25+Math.sin(pt*1.3)*Math.min(W,H)*0.12;
    const sp=3+Math.abs(Math.sin(pt*8))*4|0;
    ctx.fillStyle=`rgba(180,220,255,${0.6+Math.sin(pt*8)*0.4})`;
    ctx.beginPath();ctx.arc(px,py,sp,0,Math.PI*2);ctx.fill();
  }
  requestAnimationFrame(draw);
}
draw();

</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char LIGHTNING_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>LIGHTNING · COSMIC-S3</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);
  border-bottom:1px solid #eeeeff;padding:5px 10px;display:flex;
  justify-content:space-between;font-size:11px;z-index:9}
.nav a{{color:#eeeeff;text-decoration:none}}
canvas{display:block}
</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>LIGHTNING</span></div>
<canvas id="c"></canvas>
<script>
const C=document.getElementById('c');
const ctx=C.getContext('2d');
let W,H;
function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();init&&init();});
resize();

let bolts=[];
function bolt(x1,y1,x2,y2,depth,hue){
  if(depth<=0){return[[x1,y1,x2,y2,hue]];}
  const mx=(x1+x2)/2+(Math.random()-0.5)*(Math.abs(x2-x1)+Math.abs(y2-y1))*0.4;
  const my=(y1+y2)/2+(Math.random()-0.5)*(Math.abs(x2-x1)+Math.abs(y2-y1))*0.4;
  const forks=bolt(x1,y1,mx,my,depth-1,hue).concat(bolt(mx,my,x2,y2,depth-1,hue));
  if(depth>=2&&Math.random()<0.4){
    const fa=Math.random()*Math.PI*2;
    const fl=Math.min(W,H)*0.12;
    forks.push(...bolt(mx,my,mx+Math.cos(fa)*fl,my+Math.sin(fa)*fl,depth-2,(hue+40)%360));
  }
  return forks;
}
function newStrike(){
  const sx=W*0.1+Math.random()*W*0.8,hue=180+Math.random()*80;
  bolts.push({segs:bolt(sx,0,sx+(Math.random()-0.5)*W*0.3,H*0.5+Math.random()*H*0.4,5,hue),life:1});
}
setInterval(newStrike,300+Math.random()*400);
let t=0;
function draw(){
  t+=0.05;
  ctx.fillStyle='rgba(0,0,10,0.3)';ctx.fillRect(0,0,W,H);
  bolts=bolts.filter(b=>b.life>0);
  bolts.forEach(b=>{
    b.segs.forEach(([x1,y1,x2,y2,hue])=>{
      ctx.strokeStyle=`rgba(200,200,255,${b.life*0.7})`;ctx.lineWidth=b.life*2;
      ctx.beginPath();ctx.moveTo(x1,y1);ctx.lineTo(x2,y2);ctx.stroke();
      ctx.strokeStyle=`rgba(255,255,255,${b.life*0.9})`;ctx.lineWidth=b.life*0.5;
      ctx.beginPath();ctx.moveTo(x1,y1);ctx.lineTo(x2,y2);ctx.stroke();
    });
    b.life-=0.045;
  });
  requestAnimationFrame(draw);
}
draw();

</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char BOUNCEBALLS_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>BOUNCE BALLS · COSMIC-S3</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);
  border-bottom:1px solid #ff44cc;padding:5px 10px;display:flex;
  justify-content:space-between;font-size:11px;z-index:9}
.nav a{{color:#ff44cc;text-decoration:none}}
canvas{display:block}
</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>BOUNCE BALLS</span></div>
<canvas id="c"></canvas>
<script>
const C=document.getElementById('c');
const ctx=C.getContext('2d');
let W,H;
function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();init&&init();});
resize();

const N=18;
const bx=new Float32Array(N),by=new Float32Array(N),bvx=new Float32Array(N),bvy=new Float32Array(N);
const br=new Float32Array(N),bhue=new Float32Array(N);
function init(){for(let i=0;i<N;i++){bx[i]=Math.random()*W;by[i]=Math.random()*H;
  bvx[i]=(Math.random()-0.5)*5;bvy[i]=(Math.random()-0.5)*5;
  br[i]=8+Math.random()*16;bhue[i]=Math.random()*360;}}
let t=0;
function draw(){
  t+=0.02;
  ctx.fillStyle='rgba(0,0,0,0.12)';ctx.fillRect(0,0,W,H);
  for(let i=0;i<N;i++){
    bx[i]+=bvx[i];by[i]+=bvy[i];
    if(bx[i]<br[i]){bx[i]=br[i];bvx[i]=Math.abs(bvx[i]);}
    if(bx[i]>W-br[i]){bx[i]=W-br[i];bvx[i]=-Math.abs(bvx[i]);}
    if(by[i]<br[i]){by[i]=br[i];bvy[i]=Math.abs(bvy[i]);}
    if(by[i]>H-br[i]){by[i]=H-br[i];bvy[i]=-Math.abs(bvy[i]);}
    // ball-ball collisions
    for(let j=i+1;j<N;j++){
      const dx=bx[j]-bx[i],dy=by[j]-by[i],d=Math.sqrt(dx*dx+dy*dy);
      if(d<br[i]+br[j]&&d>0.01){
        const nx=dx/d,ny=dy/d;
        const rv=(bvx[i]-bvx[j])*nx+(bvy[i]-bvy[j])*ny;
        if(rv>0){bvx[i]-=rv*nx;bvy[i]-=rv*ny;bvx[j]+=rv*nx;bvy[j]+=rv*ny;}
      }
    }
    bhue[i]=(bhue[i]+0.3)%360;
    const g=ctx.createRadialGradient(bx[i]-br[i]*0.3,by[i]-br[i]*0.3,1,bx[i],by[i],br[i]*1.4);
    g.addColorStop(0,`hsla(${bhue[i]},100%,85%,0.9)`);
    g.addColorStop(0.5,`hsla(${bhue[i]},100%,55%,0.8)`);
    g.addColorStop(1,'transparent');
    ctx.fillStyle=g;ctx.beginPath();ctx.arc(bx[i],by[i],br[i]*1.4,0,Math.PI*2);ctx.fill();
  }
  requestAnimationFrame(draw);
}
init();draw();

</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char NEONRAIN_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>NEON RAIN · COSMIC-S3</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);
  border-bottom:1px solid #00ff88;padding:5px 10px;display:flex;
  justify-content:space-between;font-size:11px;z-index:9}
.nav a{{color:#00ff88;text-decoration:none}}
canvas{display:block}
</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>NEON RAIN</span></div>
<canvas id="c"></canvas>
<script>
const C=document.getElementById('c');
const ctx=C.getContext('2d');
let W,H;
function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();init&&init();});
resize();

const COLS2=Math.ceil(innerWidth/10)||40;
const streams2=[];
const SYM='@#%&*!?+=-~<>^[]{}|/\\';
for(let i=0;i<COLS2;i++)streams2.push({x:i*10,y:-Math.random()*400,speed:0.6+Math.random()*1.8,
  color:Math.random()<0.7?[0,255,80+Math.floor(Math.random()*60)]:[0,100+Math.floor(Math.random()*80),255],
  sym:SYM[Math.floor(Math.random()*SYM.length)]});
let t2=0;
function draw(){
  t2+=1;
  ctx.fillStyle='rgba(0,0,0,0.15)';ctx.fillRect(0,0,W,H);
  streams2.forEach(s=>{
    const [r,g,b]=s.color;
    ctx.fillStyle=`rgb(${r},${g},${b})`;
    ctx.font='bold 9px monospace';ctx.fillText(s.sym,s.x,s.y);
    s.y+=s.speed;
    if(t2%7===0)s.sym=SYM[Math.floor(Math.random()*SYM.length)];
    if(s.y>H+10){s.y=-Math.random()*200;s.speed=0.6+Math.random()*1.8;}
  });
  requestAnimationFrame(draw);
}
draw();

</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char DNA_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>DNA HELIX · COSMIC-S3</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);
  border-bottom:1px solid #ff66aa;padding:5px 10px;display:flex;
  justify-content:space-between;font-size:11px;z-index:9}
.nav a{{color:#ff66aa;text-decoration:none}}
canvas{display:block}
</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>DNA HELIX</span></div>
<canvas id="c"></canvas>
<script>
const C=document.getElementById('c');
const ctx=C.getContext('2d');
let W,H;
function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();init&&init();});
resize();

let t=0;
function draw(){
  t+=0.03;
  ctx.fillStyle='rgba(0,0,0,0.2)';ctx.fillRect(0,0,W,H);
  const cx=W/2,amp=Math.min(W,H)*0.3;
  const N=60,step=H/N;
  const pts1=[],pts2=[];
  for(let i=0;i<N;i++){
    const y=i*step,phase=i*0.22+t;
    pts1.push([cx+Math.cos(phase)*amp, y]);
    pts2.push([cx+Math.cos(phase+Math.PI)*amp, y]);
  }
  // rungs
  for(let i=0;i<N;i+=3){
    const [x1,y1]=pts1[i],[x2,y2]=pts2[i];
    const hue=(i*6+t*30)%360;
    ctx.strokeStyle=`hsla(${hue|0},100%,65%,0.55)`;ctx.lineWidth=2;
    ctx.beginPath();ctx.moveTo(x1,y1);ctx.lineTo(x2,y2);ctx.stroke();
    ctx.fillStyle=`hsla(${hue|0},100%,80%,0.9)`;
    ctx.beginPath();ctx.arc(x1,y1,4,0,Math.PI*2);ctx.fill();
    ctx.beginPath();ctx.arc(x2,y2,4,0,Math.PI*2);ctx.fill();
  }
  // strands
  [[pts1,'#ff44aa'],[pts2,'#44aaff']].forEach(([pts,col])=>{
    ctx.strokeStyle=col;ctx.lineWidth=3;ctx.shadowColor=col;ctx.shadowBlur=8;
    ctx.beginPath();pts.forEach(([x,y],i)=>i?ctx.lineTo(x,y):ctx.moveTo(x,y));ctx.stroke();
    ctx.shadowBlur=0;
  });
  requestAnimationFrame(draw);
}
draw();

</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char SANDFALL_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>SAND FALL · COSMIC-S3</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);
  border-bottom:1px solid #ffcc44;padding:5px 10px;display:flex;
  justify-content:space-between;font-size:11px;z-index:9}
.nav a{{color:#ffcc44;text-decoration:none}}
canvas{display:block}
</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>SAND FALL</span></div>
<canvas id="c"></canvas>
<script>
const C=document.getElementById('c');
const ctx=C.getContext('2d');
let W,H;
function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();init&&init();});
resize();

const SC=3;let GW,GH,grid,grid2;
function init(){GW=W/SC|0;GH=H/SC|0;grid=new Uint8Array(GW*GH);grid2=new Uint8Array(GW*GH);}
function idx(x,y){return y*GW+x;}
let t=0;
function update(){
  // spawn sand at top
  for(let i=0;i<4;i++){const x=GW*0.1+Math.random()*GW*0.8|0;if(grid[idx(x,0)]==0)grid[idx(x,0)]=1+Math.floor(Math.random()*4);}
  grid2.fill(0);
  for(let y=GH-2;y>=0;y--){for(let x=0;x<GW;x++){
    const c=grid[idx(x,y)];if(!c)continue;
    if(!grid[idx(x,y+1)]){grid2[idx(x,y+1)]=c;}
    else{const d=Math.random()<0.5?-1:1;
      if(x+d>=0&&x+d<GW&&!grid[idx(x+d,y+1)])grid2[idx(x+d,y+1)]=c;
      else grid2[idx(x,y)]=c;}
  }}
  grid.set(grid2);
  t++;if(t>400){grid.fill(0);t=0;}
}
const COLS3=['#ffdd44','#ffaa22','#ff8800','#cc6600','#ffee88'];
function draw(){
  ctx.fillStyle='#0a0500';ctx.fillRect(0,0,W,H);
  for(let y=0;y<GH;y++)for(let x=0;x<GW;x++){
    const c=grid[idx(x,y)];if(c){ctx.fillStyle=COLS3[c-1];ctx.fillRect(x*SC,y*SC,SC,SC);}
  }
  update();requestAnimationFrame(draw);
}
init();draw();

</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char ACIDSPIRAL_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ACID SPIRAL · COSMIC-S3</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);
  border-bottom:1px solid #ff00ff;padding:5px 10px;display:flex;
  justify-content:space-between;font-size:11px;z-index:9}
.nav a{{color:#ff00ff;text-decoration:none}}
canvas{display:block}
</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>ACID SPIRAL</span></div>
<canvas id="c"></canvas>
<script>
const C=document.getElementById('c');
const ctx=C.getContext('2d');
let W,H;
function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();init&&init();});
resize();

let t=0;
function draw(){
  t+=0.03;
  ctx.fillStyle='rgba(0,0,0,0.08)';ctx.fillRect(0,0,W,H);
  const cx=W/2,cy=H/2;
  for(let arm=0;arm<5;arm++){
    const armOff=arm*Math.PI*2/5;
    for(let i=0;i<200;i++){
      const theta=i*0.12+t+armOff;
      const r=i*Math.min(W,H)*0.0013;
      const x=cx+Math.cos(theta)*r*Math.min(W,H)*0.45;
      const y=cy+Math.sin(theta)*r*Math.min(W,H)*0.45;
      const hue=(i*2.5+t*60+arm*72)%360;
      const sz=1+Math.sin(i*0.18+t*3)*1.5;
      ctx.fillStyle=`hsla(${hue|0},100%,65%,0.7)`;
      ctx.beginPath();ctx.arc(x,y,sz,0,Math.PI*2);ctx.fill();
    }
  }
  requestAnimationFrame(draw);
}
draw();

</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char PLASMAGLOBE_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>PLASMA GLOBE · COSMIC-S3</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);
  border-bottom:1px solid #ff8800;padding:5px 10px;display:flex;
  justify-content:space-between;font-size:11px;z-index:9}
.nav a{{color:#ff8800;text-decoration:none}}
canvas{display:block}
</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>PLASMA GLOBE</span></div>
<canvas id="c"></canvas>
<script>
const C=document.getElementById('c');
const ctx=C.getContext('2d');
let W,H;
function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();init&&init();});
resize();

const S2=4;let IW2,IH2,ofc2,octx2;
function init(){IW2=W/S2|0;IH2=H/S2|0;ofc2=new OffscreenCanvas(IW2,IH2);octx2=ofc2.getContext('2d');}
// tendrils
const TD=8;const tseeds=Array.from({length:TD},(_,i)=>({a:i*Math.PI*2/TD,spd:0.4+Math.random()*0.6}));
let t=0;
function draw(){
  t+=0.025;
  ctx.fillStyle='rgba(0,0,0,0.25)';ctx.fillRect(0,0,W,H);
  const cx=W/2,cy=H/2,glob=Math.min(W,H)*0.14;
  // tendrils
  tseeds.forEach((td,k)=>{
    td.a+=td.spd*0.015;
    const pts=[];
    for(let s=0;s<40;s++){
      const frac=s/39;
      const r=glob+frac*(Math.min(W,H)*0.38);
      const wob=Math.sin(t*3+k+s*0.4)*0.35;
      const a=td.a+wob*frac;
      pts.push([cx+Math.cos(a)*r,cy+Math.sin(a)*r]);
    }
    const hue=(k*42+t*25)%360;
    ctx.strokeStyle=`hsla(${hue|0},100%,70%,0.6)`;ctx.lineWidth=2;
    ctx.shadowColor=`hsla(${hue|0},100%,70%,1)`;ctx.shadowBlur=10;
    ctx.beginPath();pts.forEach(([x,y],i)=>i?ctx.lineTo(x,y):ctx.moveTo(x,y));ctx.stroke();
    ctx.shadowBlur=0;
  });
  // globe
  const gg=ctx.createRadialGradient(cx,cy,0,cx,cy,glob);
  gg.addColorStop(0,'rgba(255,255,255,0.9)');gg.addColorStop(0.3,`rgba(180,120,255,0.6)`);gg.addColorStop(1,'rgba(80,40,120,0.2)');
  ctx.fillStyle=gg;ctx.beginPath();ctx.arc(cx,cy,glob,0,Math.PI*2);ctx.fill();
  ctx.strokeStyle='rgba(200,180,255,0.5)';ctx.lineWidth=2;ctx.beginPath();ctx.arc(cx,cy,glob,0,Math.PI*2);ctx.stroke();
  requestAnimationFrame(draw);
}
init();draw();

</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char WARPGRID_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>WARP GRID · COSMIC-S3</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);
  border-bottom:1px solid #00ccff;padding:5px 10px;display:flex;
  justify-content:space-between;font-size:11px;z-index:9}
.nav a{{color:#00ccff;text-decoration:none}}
canvas{display:block}
</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>WARP GRID</span></div>
<canvas id="c"></canvas>
<script>
const C=document.getElementById('c');
const ctx=C.getContext('2d');
let W,H;
function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();init&&init();});
resize();

let t=0;
const GX=24,GY=18;
function draw(){
  t+=0.025;
  ctx.fillStyle='rgba(0,0,10,0.3)';ctx.fillRect(0,0,W,H);
  function pt(gx,gy){
    const nx=gx/GX*2-1,ny=gy/GY*2-1;
    const d=Math.sqrt(nx*nx+ny*ny);
    const wave=Math.sin(d*4-t*3)*0.15;
    const twist=Math.sin(t*0.8+d)*0.2;
    const sx=nx+wave*Math.cos(t+gy)+twist*ny;
    const sy=ny+wave*Math.sin(t+gx)-twist*nx;
    return[(sx*0.45+0.5)*W,(sy*0.45+0.5)*H];
  }
  for(let gy=0;gy<=GY;gy++){
    const hue=(gy/GY*120+t*20)%360;
    ctx.strokeStyle=`hsla(${hue|0},100%,60%,0.65)`;ctx.lineWidth=1;
    ctx.beginPath();
    for(let gx=0;gx<=GX;gx++){const[x,y]=pt(gx,gy);gx?ctx.lineTo(x,y):ctx.moveTo(x,y);}ctx.stroke();
  }
  for(let gx=0;gx<=GX;gx++){
    const hue=(gx/GX*120+180+t*20)%360;
    ctx.strokeStyle=`hsla(${hue|0},100%,60%,0.65)`;ctx.lineWidth=1;
    ctx.beginPath();
    for(let gy=0;gy<=GY;gy++){const[x,y]=pt(gx,gy);gy?ctx.lineTo(x,y):ctx.moveTo(x,y);}ctx.stroke();
  }
  requestAnimationFrame(draw);
}
draw();

</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char NEBULA_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>NEBULA · COSMIC-S3</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);
  border-bottom:1px solid #ff44ff;padding:5px 10px;display:flex;
  justify-content:space-between;font-size:11px;z-index:9}
.nav a{{color:#ff44ff;text-decoration:none}}
canvas{display:block}
</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>NEBULA</span></div>
<canvas id="c"></canvas>
<script>
const C=document.getElementById('c');
const ctx=C.getContext('2d');
let W,H;
function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();init&&init();});
resize();

const S3=3;let IW3,IH3,ofc3,octx3;
function init(){IW3=W/S3|0;IH3=H/S3|0;ofc3=new OffscreenCanvas(IW3,IH3);octx3=ofc3.getContext('2d');}
const NS=80;const nx_=new Float32Array(NS),ny_=new Float32Array(NS),nbr=new Float32Array(NS);
for(let i=0;i<NS;i++){nx_[i]=Math.random();ny_[i]=Math.random();nbr[i]=60+Math.random()*195;}
let t=0;
function draw(){
  t+=0.018;
  const id=octx3.createImageData(IW3,IH3);const d=id.data;
  for(let y=0;y<IH3;y++)for(let x=0;x<IW3;x++){
    const fx=x/IW3,fy=y/IH3;
    let r=0,g=0,b=0;
    for(let s=0;s<NS;s++){
      const dx=(fx-nx_[s])*2.5,dy=(fy-ny_[s])*2;
      const dist=Math.sqrt(dx*dx+dy*dy)+0.001;
      const hue=(s*137.5+t*20)%360;
      const c=Math.max(0,nbr[s]/(dist*dist*400)-0.01);
      r+=c*Math.sin(hue/57.3)*127;g+=c*Math.sin((hue+120)/57.3)*127;b+=c*Math.sin((hue+240)/57.3)*127;
    }
    const idx2=(y*IW3+x)*4;
    d[idx2]=Math.min(255,r+30)|0;d[idx2+1]=Math.min(255,g+10)|0;d[idx2+2]=Math.min(255,b+40)|0;d[idx2+3]=255;
  }
  octx3.putImageData(id,0,0);
  ctx.drawImage(ofc3,0,0,W,H);
  // foreground stars
  for(let i=0;i<NS;i++){
    const sx=nx_[i]*W,sy=ny_[i]*H;
    const tw=0.5+Math.sin(t*2+i)*0.5;
    ctx.fillStyle=`rgba(255,255,255,${tw*0.8})`;
    ctx.beginPath();ctx.arc(sx,sy,1+tw,0,Math.PI*2);ctx.fill();
  }
  requestAnimationFrame(draw);
}
init();draw();

</script>
</body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char APOLLONIAN_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>APOLLONIAN · COSMIC-S3</title><style>*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);border-bottom:1px solid #ff8844;padding:5px 10px;display:flex;justify-content:space-between;font-size:11px;z-index:9}.nav a{color:#ff8844;text-decoration:none}
canvas{display:block}</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>APOLLONIAN</span></div><canvas id="c"></canvas>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
let W,H;function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();if(typeof init==='function')init();});
resize();

const circles=[];const MAXK=350;
function csqrt(re,im){const r=Math.sqrt(Math.sqrt(re*re+im*im)),t=Math.atan2(im,re)/2;return[r*Math.cos(t),r*Math.sin(t)];}
function findNew(c1,c2,c3){
  const[k1,x1,y1]=c1,[k2,x2,y2]=c2,[k3,x3,y3]=c3;
  const disc=k1*k2+k2*k3+k3*k1;if(disc<0)return null;
  const k4=k1+k2+k3+2*Math.sqrt(disc);if(k4<=0||k4>MAXK)return null;
  const[sqr,sqi]=csqrt(k1*k2*(x1*x2-y1*y2)+k2*k3*(x2*x3-y2*y3)+k3*k1*(x3*x1-y3*y1),
                        k1*k2*(x1*y2+x2*y1)+k2*k3*(x2*y3+x3*y2)+k3*k1*(x3*y1+x1*y3));
  const nr=(k1*x1+k2*x2+k3*x3+2*sqr)/k4,ni=(k1*y1+k2*y2+k3*y3+2*sqi)/k4;
  if(Math.sqrt(nr*nr+ni*ni)+1/k4>1.04)return null;
  return[k4,nr,ni];
}
function build(){
  circles.length=0;
  const k3i=1+2/Math.sqrt(3),d=1-1/k3i;
  const outer=[-1,0,0],a=[k3i,d,0],b=[k3i,d*Math.cos(2.094),d*Math.sin(2.094)],c=[k3i,d*Math.cos(4.189),d*Math.sin(4.189)];
  circles.push(outer,a,b,c);
  const queue=[[outer,a,b],[outer,b,c],[outer,c,a],[a,b,c]];
  const seen=new Set();
  for(let qi=0;qi<queue.length&&circles.length<560;qi++){
    const[c1,c2,c3]=queue[qi];const nc=findNew(c1,c2,c3);if(!nc)continue;
    const key=nc[0].toFixed(1)+','+nc[1].toFixed(3)+','+nc[2].toFixed(3);
    if(seen.has(key))continue;seen.add(key);circles.push(nc);
    if(nc[0]<MAXK*0.7)queue.push([c1,c2,nc],[c1,c3,nc],[c2,c3,nc]);
  }
}
let t=0;
function draw(){
  t+=0.012;ctx.fillStyle='rgba(0,0,0,0.1)';ctx.fillRect(0,0,W,H);
  const R=Math.min(W,H)*0.46,cx=W/2,cy=H/2;
  circles.forEach(([k,x,y])=>{
    if(k<0){ctx.strokeStyle='rgba(150,100,255,0.35)';ctx.lineWidth=2;ctx.beginPath();ctx.arc(cx,cy,R,0,Math.PI*2);ctx.stroke();return;}
    const r=R/k,px=cx+x*R,py=cy+y*R,hue=(Math.log(k)*60+t*25)%360;
    ctx.fillStyle=`hsla(${hue|0},100%,65%,${Math.min(1,r/3+0.3)})`;
    ctx.beginPath();ctx.arc(px,py,Math.max(0.8,r),0,Math.PI*2);ctx.fill();
    if(r>3){ctx.strokeStyle=`hsla(${hue|0},100%,88%,0.35)`;ctx.lineWidth=0.5;ctx.stroke();}
  });
  requestAnimationFrame(draw);
}
build();draw();

</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char SUNFLOWER_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>SUNFLOWER · COSMIC-S3</title><style>*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);border-bottom:1px solid #ffdd00;padding:5px 10px;display:flex;justify-content:space-between;font-size:11px;z-index:9}.nav a{color:#ffdd00;text-decoration:none}
canvas{display:block}</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>SUNFLOWER</span></div><canvas id="c"></canvas>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
let W,H;function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();if(typeof init==='function')init();});
resize();

const N=900;let t=0;
function draw(){
  t+=0.02;ctx.fillStyle='rgba(0,0,0,0.1)';ctx.fillRect(0,0,W,H);
  const cx=W/2,cy=H/2,sc=Math.min(W,H)*0.47/Math.sqrt(N);
  for(let i=0;i<N;i++){
    const theta=i*2.39996317;const r=Math.sqrt(i)*sc;
    const x=cx+r*Math.cos(theta),y=cy+r*Math.sin(theta);
    const hue=(i*0.4+t*20)%360,pulse=0.5+0.5*Math.sin(t*2+i*0.05);
    ctx.fillStyle=`hsla(${hue|0},100%,${50+pulse*30}%,0.85)`;
    ctx.beginPath();ctx.arc(x,y,1.5+pulse*2,0,Math.PI*2);ctx.fill();
  }
  requestAnimationFrame(draw);
}
draw();

</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char QUASICRYSTAL_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>QUASICRYSTAL · COSMIC-S3</title><style>*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);border-bottom:1px solid #88ffff;padding:5px 10px;display:flex;justify-content:space-between;font-size:11px;z-index:9}.nav a{color:#88ffff;text-decoration:none}
canvas{display:block}</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>QUASICRYSTAL</span></div><canvas id="c"></canvas>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
let W,H;function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();if(typeof init==='function')init();});
resize();

const S=4;let IW,IH,ofc,octx;
const NWAVES=5;
function init(){IW=W/S|0;IH=H/S|0;ofc=new OffscreenCanvas(IW,IH);octx=ofc.getContext('2d');}
let t=0;
function draw(){
  t+=0.018;
  const id=octx.createImageData(IW,IH);const d=id.data;
  for(let py=0;py<IH;py++)for(let px=0;px<IW;px++){
    let sum=0;
    for(let k=0;k<NWAVES;k++){const a=k*Math.PI/NWAVES;sum+=Math.cos(Math.cos(a)*px*0.11+Math.sin(a)*py*0.11+t);}
    const n=(sum/NWAVES+1)/2;
    const hue=(n*360+t*30)%360;
    const i=(py*IW+px)*4;
    d[i]=Math.sin(hue/57.3)*127+128|0;d[i+1]=Math.sin((hue+120)/57.3)*127+128|0;
    d[i+2]=Math.sin((hue+240)/57.3)*127+128|0;d[i+3]=255;
  }
  octx.putImageData(id,0,0);ctx.drawImage(ofc,0,0,W,H);
  requestAnimationFrame(draw);
}
init();draw();

</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char LORENZ_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>LORENZ · COSMIC-S3</title><style>*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);border-bottom:1px solid #00ffdd;padding:5px 10px;display:flex;justify-content:space-between;font-size:11px;z-index:9}.nav a{color:#00ffdd;text-decoration:none}
canvas{display:block}</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>LORENZ</span></div><canvas id="c"></canvas>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
let W,H;function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();if(typeof init==='function')init();});
resize();

let lx=0.1,ly=0,lz=0;
const sig=10,rho=28,beta=2.667;
const pts=[];const MAXPTS=3500;
let t=0;
function init(){pts.length=0;lx=0.1+Math.random()*0.1;ly=0;lz=0;}
function draw(){
  t+=0.016;
  for(let s=0;s<10;s++){
    const dt=0.005;
    const dx=sig*(ly-lx),dy=lx*(rho-lz)-ly,dz=lx*ly-beta*lz;
    lx+=dx*dt;ly+=dy*dt;lz+=dz*dt;
    pts.push([lx,lz]);if(pts.length>MAXPTS)pts.shift();
  }
  ctx.fillStyle='rgba(0,0,8,0.12)';ctx.fillRect(0,0,W,H);
  const cx=W/2,cy=H/2,sc=Math.min(W,H)/(rho*1.9);
  for(let i=1;i<pts.length;i++){
    const[x1,z1]=pts[i-1],[x2,z2]=pts[i];
    const hue=(i/MAXPTS*280+t*20)%360;
    ctx.strokeStyle=`hsla(${hue|0},100%,65%,0.65)`;ctx.lineWidth=1;
    ctx.beginPath();ctx.moveTo(cx+x1*sc,cy+(z1-rho/2)*sc);ctx.lineTo(cx+x2*sc,cy+(z2-rho/2)*sc);ctx.stroke();
  }
  requestAnimationFrame(draw);
}
init();draw();

</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char MANDELBROT_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>MANDELBROT · COSMIC-S3</title><style>*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);border-bottom:1px solid #ffaaff;padding:5px 10px;display:flex;justify-content:space-between;font-size:11px;z-index:9}.nav a{color:#ffaaff;text-decoration:none}
canvas{display:block}</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>MANDELBROT</span></div><canvas id="c"></canvas>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
let W,H;function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();if(typeof init==='function')init();});
resize();

const S=5;let IW,IH,ofc,octx,iters=null;
const MAXIT=64;
function init(){
  IW=W/S|0;IH=H/S|0;ofc=new OffscreenCanvas(IW,IH);octx=ofc.getContext('2d');
  iters=new Float32Array(IW*IH);
  const zoom=2.6,cxm=-0.7,cyi=0.0;const sc=zoom/Math.min(IW,IH);
  for(let py=0;py<IH;py++)for(let px=0;px<IW;px++){
    const c0r=cxm+(px-IW/2)*sc,c0i=cyi+(py-IH/2)*sc;
    let zr=0,zi=0,it=0;
    while(zr*zr+zi*zi<4&&it<MAXIT){const tr=zr*zr-zi*zi+c0r;zi=2*zr*zi+c0i;zr=tr;it++;}
    iters[py*IW+px]=it===MAXIT?-1:it+1-Math.log2(Math.log2(Math.sqrt(zr*zr+zi*zi)));
  }
}
let t=0;
function draw(){
  t+=0.35;
  if(iters){
    const id=octx.createImageData(IW,IH);const d=id.data;
    for(let i=0;i<iters.length;i++){
      const n=iters[i];const idx=i*4;
      if(n<0){d[idx+3]=255;continue;}
      const hue=(n/MAXIT*360+t)%360;
      d[idx]=Math.sin(hue/57.3)*127+128|0;d[idx+1]=Math.sin((hue+120)/57.3)*127+128|0;
      d[idx+2]=Math.sin((hue+240)/57.3)*127+128|0;d[idx+3]=255;
    }
    octx.putImageData(id,0,0);ctx.drawImage(ofc,0,0,W,H);
  }
  requestAnimationFrame(draw);
}
init();draw();

</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char REACTION_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>REACTION · COSMIC-S3</title><style>*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);border-bottom:1px solid #44ffaa;padding:5px 10px;display:flex;justify-content:space-between;font-size:11px;z-index:9}.nav a{color:#44ffaa;text-decoration:none}
canvas{display:block}</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>REACTION</span></div><canvas id="c"></canvas>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
let W,H;function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();if(typeof init==='function')init();});
resize();

const GW=100,GH=75;
const Du=0.2097,Dv=0.105,F=0.055,K=0.062;
let u,v,nu,nv,ofc,octx;
function init(){
  u=new Float32Array(GW*GH);v=new Float32Array(GW*GH);
  nu=new Float32Array(GW*GH);nv=new Float32Array(GW*GH);
  ofc=new OffscreenCanvas(GW,GH);octx=ofc.getContext('2d');
  u.fill(1);
  for(let i=0;i<10;i++){
    const x=Math.random()*GW|0,y=Math.random()*GH|0;
    for(let dy=-3;dy<=3;dy++)for(let dx=-3;dx<=3;dx++){
      const nx=(x+dx+GW)%GW,ny=(y+dy+GH)%GH;
      u[ny*GW+nx]=0.5;v[ny*GW+nx]=0.25+Math.random()*0.5;
    }
  }
}
function step(){
  for(let y=0;y<GH;y++)for(let x=0;x<GW;x++){
    const i=y*GW+x;
    const xm=(x-1+GW)%GW,xp=(x+1)%GW,ym=(y-1+GH)%GH,yp=(y+1)%GH;
    const lapu=u[y*GW+xm]+u[y*GW+xp]+u[ym*GW+x]+u[yp*GW+x]-4*u[i];
    const lapv=v[y*GW+xm]+v[y*GW+xp]+v[ym*GW+x]+v[yp*GW+x]-4*v[i];
    const uvv=u[i]*v[i]*v[i];
    nu[i]=Math.max(0,Math.min(1,u[i]+Du*lapu-uvv+F*(1-u[i])));
    nv[i]=Math.max(0,Math.min(1,v[i]+Dv*lapv+uvv-(F+K)*v[i]));
  }
  let tmp;tmp=u;u=nu;nu=tmp;tmp=v;v=nv;nv=tmp;
}
let t=0;
function draw(){
  t++;step();step();
  const id=octx.createImageData(GW,GH);const d=id.data;
  for(let i=0;i<GW*GH;i++){
    const vi=v[i];
    d[i*4]=vi>0.5?((vi-0.5)*510)|0:0;
    d[i*4+1]=Math.min(255,vi*400)|0;
    d[i*4+2]=Math.max(0,255-vi*500)|0;
    d[i*4+3]=255;
  }
  octx.putImageData(id,0,0);ctx.drawImage(ofc,0,0,W,H);
  if(t>700){t=0;init();}
  requestAnimationFrame(draw);
}
init();draw();

</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char MAZE_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>MAZE · COSMIC-S3</title><style>*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);border-bottom:1px solid #00aaff;padding:5px 10px;display:flex;justify-content:space-between;font-size:11px;z-index:9}.nav a{color:#00aaff;text-decoration:none}
canvas{display:block}</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>MAZE</span></div><canvas id="c"></canvas>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
let W,H;function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();if(typeof init==='function')init();});
resize();

const MW=40,MH=30;
let walls,visited,stack,carving;
function init(){
  walls=new Uint8Array(MW*MH).fill(0xF);
  visited=new Uint8Array(MW*MH);
  stack=[];carving=true;
  const sx=MW/2|0,sy=MH/2|0;
  visited[sy*MW+sx]=1;stack.push([sx,sy]);
}
function carveStep(){
  if(!stack.length){carving=false;return;}
  const[cx,cy]=stack[stack.length-1];
  const nb=[];
  if(cy>0&&!visited[(cy-1)*MW+cx])nb.push([cx,cy-1,0,4]);
  if(cx<MW-1&&!visited[cy*MW+cx+1])nb.push([cx+1,cy,2,8]);
  if(cy<MH-1&&!visited[(cy+1)*MW+cx])nb.push([cx,cy+1,4,1]);
  if(cx>0&&!visited[cy*MW+cx-1])nb.push([cx-1,cy,8,2]);
  if(!nb.length){stack.pop();return;}
  const[nx,ny,wc,wn]=nb[Math.random()*nb.length|0];
  walls[cy*MW+cx]&=~wc;walls[ny*MW+nx]&=~wn;
  visited[ny*MW+nx]=1;stack.push([nx,ny]);
}
let t=0;
function draw(){
  t++;if(carving)for(let s=0;s<4;s++)carveStep();
  ctx.fillStyle='#000';ctx.fillRect(0,0,W,H);
  const cw=W/MW,ch=H/MH;
  ctx.strokeStyle='#0088ff';ctx.lineWidth=1.5;
  for(let y=0;y<MH;y++)for(let x=0;x<MW;x++){
    const w=walls[y*MW+x],px=x*cw,py=y*ch;
    if(w&1){ctx.beginPath();ctx.moveTo(px,py);ctx.lineTo(px+cw,py);ctx.stroke();}
    if(w&2){ctx.beginPath();ctx.moveTo(px+cw,py);ctx.lineTo(px+cw,py+ch);ctx.stroke();}
    if(w&4){ctx.beginPath();ctx.moveTo(px,py+ch);ctx.lineTo(px+cw,py+ch);ctx.stroke();}
    if(w&8){ctx.beginPath();ctx.moveTo(px,py);ctx.lineTo(px,py+ch);ctx.stroke();}
  }
  if(stack.length){
    const[cx,cy]=stack[stack.length-1];
    ctx.fillStyle='rgba(0,255,120,0.55)';ctx.fillRect(cx*cw+2,cy*ch+2,cw-4,ch-4);
  }
  if(!carving&&t%280===0)init();
  requestAnimationFrame(draw);
}
init();draw();

</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char VINES_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>VINES · COSMIC-S3</title><style>*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);border-bottom:1px solid #44ff88;padding:5px 10px;display:flex;justify-content:space-between;font-size:11px;z-index:9}.nav a{color:#44ff88;text-decoration:none}
canvas{display:block}</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>VINES</span></div><canvas id="c"></canvas>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
let W,H;function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();if(typeof init==='function')init();});
resize();

const segs=[];
function branch(x,y,angle,len,depth,hue){
  if(depth<=0||len<3)return;
  const x2=x+Math.cos(angle)*len,y2=y+Math.sin(angle)*len;
  segs.push({x1:x,y1:y,x2,y2,hue,depth,drawn:false});
  const spread=0.35+Math.random()*0.25;
  branch(x2,y2,angle-spread,len*0.72,depth-1,(hue+15)%360);
  branch(x2,y2,angle+spread,len*0.72,depth-1,(hue+30)%360);
  if(depth>3&&Math.random()<0.4)branch(x2,y2,angle+(Math.random()-0.5)*1.2,len*0.55,depth-2,(hue+45)%360);
}
function init(){
  segs.length=0;
  ctx.fillStyle='#000';ctx.fillRect(0,0,W,H);
  const roots=3+Math.floor(Math.random()*3);
  for(let r=0;r<roots;r++)branch(W*(0.2+r*0.3),H,-Math.PI/2+( Math.random()-0.5)*0.5,Math.min(W,H)*0.18,8,80+r*40);
  segs.forEach(s=>s.drawn=false);
}
let idx=0,t=0;
function draw(){
  t++;
  const perFrame=3;
  for(let i=0;i<perFrame&&idx<segs.length;i++,idx++){
    const s=segs[idx];
    ctx.strokeStyle=`hsla(${s.hue|0},80%,${35+s.depth*5}%,0.85)`;
    ctx.lineWidth=Math.max(0.5,s.depth*0.4);
    ctx.lineCap='round';
    ctx.beginPath();ctx.moveTo(s.x1,s.y1);ctx.lineTo(s.x2,s.y2);ctx.stroke();
    // tiny leaf at branch tips
    if(s.depth<=2){
      ctx.fillStyle=`hsla(${(s.hue+20)|0},90%,50%,0.7)`;
      ctx.beginPath();ctx.ellipse(s.x2,s.y2,4,2,Math.atan2(s.y2-s.y1,s.x2-s.x1),0,Math.PI*2);ctx.fill();
    }
  }
  if(idx>=segs.length&&t%200===0){idx=0;init();}
  requestAnimationFrame(draw);
}
init();draw();

</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char SNOWFLAKES_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>SNOWFLAKES · COSMIC-S3</title><style>*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);border-bottom:1px solid #cceeff;padding:5px 10px;display:flex;justify-content:space-between;font-size:11px;z-index:9}.nav a{color:#cceeff;text-decoration:none}
canvas{display:block}</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>SNOWFLAKES</span></div><canvas id="c"></canvas>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
let W,H;function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();if(typeof init==='function')init();});
resize();

function makeFlake(){
  const arms=[];const branches=2+Math.floor(Math.random()*3);
  const mainLen=20+Math.random()*30;
  for(let b=0;b<branches;b++)arms.push({t:0.2+b*0.2,len:mainLen*(0.3+Math.random()*0.4),angle:Math.PI/4+Math.random()*Math.PI/6});
  return{x:Math.random()*innerWidth,y:-30-Math.random()*200,vy:0.3+Math.random()*0.8,vx:(Math.random()-0.5)*0.4,mainLen,arms,rot:Math.random()*Math.PI/3,spin:(Math.random()-0.5)*0.005,size:0.5+Math.random()*1.2};
}
const flakes=Array.from({length:10},makeFlake);
function drawFlake(f){
  ctx.save();ctx.translate(f.x,f.y);ctx.rotate(f.rot);ctx.scale(f.size,f.size);
  for(let spoke=0;spoke<6;spoke++){
    ctx.save();ctx.rotate(spoke*Math.PI/3);
    ctx.strokeStyle='rgba(200,230,255,0.85)';ctx.lineWidth=1.5;ctx.lineCap='round';
    ctx.beginPath();ctx.moveTo(0,0);ctx.lineTo(0,-f.mainLen);ctx.stroke();
    f.arms.forEach(a=>{
      const ty=-f.mainLen*a.t;
      ctx.beginPath();ctx.moveTo(0,ty);ctx.lineTo(Math.sin(a.angle)*a.len,ty-Math.cos(a.angle)*a.len);ctx.stroke();
      ctx.beginPath();ctx.moveTo(0,ty);ctx.lineTo(-Math.sin(a.angle)*a.len,ty-Math.cos(a.angle)*a.len);ctx.stroke();
    });
    ctx.restore();
  }
  ctx.fillStyle='rgba(220,240,255,0.9)';ctx.beginPath();ctx.arc(0,0,3,0,Math.PI*2);ctx.fill();
  ctx.restore();
}
let t=0;
function draw(){
  t++;ctx.fillStyle='rgba(0,5,20,0.25)';ctx.fillRect(0,0,W,H);
  flakes.forEach(f=>{
    f.x+=f.vx;f.y+=f.vy;f.rot+=f.spin;
    if(f.y>H+40)Object.assign(f,makeFlake(),{x:Math.random()*W,y:-40});
    drawFlake(f);
  });
  requestAnimationFrame(draw);
}
draw();

</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char CUBE3D_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>CUBE 3D · COSMIC-S3</title><style>*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);border-bottom:1px solid #88ccff;padding:5px 10px;display:flex;justify-content:space-between;font-size:11px;z-index:9}.nav a{color:#88ccff;text-decoration:none}
canvas{display:block}</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>CUBE 3D</span></div><canvas id="c"></canvas>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
let W,H;function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();if(typeof init==='function')init();});
resize();

const verts=[[-1,-1,-1],[1,-1,-1],[1,1,-1],[-1,1,-1],[-1,-1,1],[1,-1,1],[1,1,1],[-1,1,1]];
const edges=[[0,1],[1,2],[2,3],[3,0],[4,5],[5,6],[6,7],[7,4],[0,4],[1,5],[2,6],[3,7]];
const icoV=(()=>{const t=(1+Math.sqrt(5))/2,n=Math.sqrt(1+t*t);
  return[[-1,t,0],[1,t,0],[-1,-t,0],[1,-t,0],[0,-1,t],[0,1,t],[0,-1,-t],[0,1,-t],[t,0,-1],[t,0,1],[-t,0,-1],[-t,0,1]].map(v=>v.map(x=>x/n));})();
const icoE=[[0,1],[0,5],[0,7],[0,10],[0,11],[1,5],[1,7],[1,8],[1,9],[2,3],[2,4],[2,6],[2,10],[2,11],[3,4],[3,6],[3,8],[3,9],[4,5],[4,9],[4,11],[5,9],[5,11],[6,7],[6,8],[6,10],[7,8],[7,10],[8,9],[10,11]];
let rx=0,ry=0,rz=0;let showIco=false;let switchT=0;
function rot3(v,ax,ay,az){
  let[x,y,z]=v;
  let t=x;x=x*Math.cos(az)-y*Math.sin(az);y=t*Math.sin(az)+y*Math.cos(az);
  t=x;x=x*Math.cos(ay)+z*Math.sin(ay);z=-t*Math.sin(ay)+z*Math.cos(ay);
  t=y;y=y*Math.cos(ax)-z*Math.sin(ax);z=t*Math.sin(ax)+z*Math.cos(ax);
  return[x,y,z];
}
function proj([x,y,z]){const f=W*0.55/(z+3.5);return[W/2+x*f,H/2+y*f,z];}
let t=0;
function draw(){
  t+=0.015;rx+=0.008;ry+=0.013;rz+=0.005;
  switchT++;if(switchT>300){showIco=!showIco;switchT=0;}
  ctx.fillStyle='rgba(0,0,10,0.25)';ctx.fillRect(0,0,W,H);
  const sc=Math.min(W,H)*0.22;
  const V=showIco?icoV:verts,E=showIco?icoE:edges;
  const projected=V.map(v=>{const[x,y,z]=rot3(v,rx,ry,rz);return proj([x*sc,y*sc,z*sc]);});
  E.forEach(([a,b])=>{
    const[x1,y1,z1]=projected[a],[x2,y2,z2]=projected[b];
    const depth=(z1+z2)/2;const hue=(depth/sc*60+t*40+200)%360;
    ctx.strokeStyle=`hsla(${hue|0},100%,65%,0.8)`;ctx.lineWidth=1.5;
    ctx.beginPath();ctx.moveTo(x1,y1);ctx.lineTo(x2,y2);ctx.stroke();
  });
  projected.forEach(([x,y])=>{ctx.fillStyle='#fff';ctx.beginPath();ctx.arc(x,y,2,0,Math.PI*2);ctx.fill();});
  requestAnimationFrame(draw);
}
draw();

</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char TORUS_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>TORUS · COSMIC-S3</title><style>*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);border-bottom:1px solid #ff66ff;padding:5px 10px;display:flex;justify-content:space-between;font-size:11px;z-index:9}.nav a{color:#ff66ff;text-decoration:none}
canvas{display:block}</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>TORUS</span></div><canvas id="c"></canvas>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
let W,H;function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();if(typeof init==='function')init();});
resize();

let rx=0,ry=0;
function rot3(x,y,z,ax,ay){
  let ty=y*Math.cos(ax)-z*Math.sin(ax),tz=y*Math.sin(ax)+z*Math.cos(ax);y=ty;z=tz;
  let tx=x*Math.cos(ay)+z*Math.sin(ay);tz=-x*Math.sin(ay)+z*Math.cos(ay);x=tx;z=tz;
  return[x,y,z];
}
function proj(x,y,z){const f=W*0.5/(z+4);return[W/2+x*f,H/2+y*f];}
const R=1,r=0.4,NU=32,NV=20;
let t=0;
function draw(){
  t+=0.016;rx+=0.009;ry+=0.014;
  ctx.fillStyle='rgba(0,0,0,0.2)';ctx.fillRect(0,0,W,H);
  const sc=Math.min(W,H)*0.28;
  for(let iu=0;iu<NU;iu++){
    const u=iu/NU*Math.PI*2;
    ctx.beginPath();let first=true;
    for(let iv=0;iv<=NV;iv++){
      const v=iv/NV*Math.PI*2;
      const x=(R+r*Math.cos(v))*Math.cos(u),y=(R+r*Math.cos(v))*Math.sin(u),z=r*Math.sin(v);
      const[rx2,ry2,rz2]=rot3(x*sc,y*sc,z*sc,rx,ry);
      const[px,py]=proj(rx2,ry2,rz2);
      const hue=(iu/NU*360+t*25)%360;
      if(first){ctx.strokeStyle=`hsla(${hue|0},100%,65%,0.7)`;ctx.lineWidth=1;ctx.moveTo(px,py);first=false;}
      else ctx.lineTo(px,py);
    }
    ctx.stroke();
  }
  for(let iv=0;iv<NV;iv++){
    const v=iv/NV*Math.PI*2;
    ctx.beginPath();let first=true;
    for(let iu=0;iu<=NU;iu++){
      const u=iu/NU*Math.PI*2;
      const x=(R+r*Math.cos(v))*Math.cos(u),y=(R+r*Math.cos(v))*Math.sin(u),z=r*Math.sin(v);
      const[rx2,ry2,rz2]=rot3(x*sc,y*sc,z*sc,rx,ry);
      const[px,py]=proj(rx2,ry2,rz2);
      const hue=(iv/NV*360+180+t*25)%360;
      if(first){ctx.strokeStyle=`hsla(${hue|0},100%,60%,0.5)`;ctx.lineWidth=0.8;ctx.moveTo(px,py);first=false;}
      else ctx.lineTo(px,py);
    }
    ctx.stroke();
  }
  requestAnimationFrame(draw);
}
draw();

</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char HYPERCUBE_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>HYPERCUBE · COSMIC-S3</title><style>*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);border-bottom:1px solid #ffcc00;padding:5px 10px;display:flex;justify-content:space-between;font-size:11px;z-index:9}.nav a{color:#ffcc00;text-decoration:none}
canvas{display:block}</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>HYPERCUBE</span></div><canvas id="c"></canvas>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
let W,H;function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();if(typeof init==='function')init();});
resize();

// 4D tesseract: 16 vertices, 32 edges
const verts4=[];
for(let i=0;i<16;i++)verts4.push([(i>>3&1)*2-1,(i>>2&1)*2-1,(i>>1&1)*2-1,(i&1)*2-1]);
const edges4=[];
for(let a=0;a<16;a++)for(let b=a+1;b<16;b++){
  let diff=0;for(let d=0;d<4;d++)if(verts4[a][d]!==verts4[b][d])diff++;
  if(diff===1)edges4.push([a,b]);
}
let a1=0,a2=0,a3=0,a4=0;
function rot4(v,t){
  let[x,y,z,w]=v;
  // rotate in xw and yz planes
  let tx=x*Math.cos(a1)-w*Math.sin(a1),tw=x*Math.sin(a1)+w*Math.cos(a1);x=tx;w=tw;
  let ty=y*Math.cos(a2)-z*Math.sin(a2),tz=y*Math.sin(a2)+z*Math.cos(a2);y=ty;z=tz;
  let tx2=x*Math.cos(a3)-y*Math.sin(a3),ty2=x*Math.sin(a3)+y*Math.cos(a3);x=tx2;y=ty2;
  return[x,y,z,w];
}
function proj4to2(v){
  const[x,y,z,w]=rot4(v);
  const f4=2/(w+2.5);const x3=x*f4,y3=y*f4,z3=z*f4;
  const f3=W*0.35/(z3+2.5);return[W/2+x3*f3,H/2+y3*f3,z3];
}
let t=0;
function draw(){
  t+=0.012;a1+=0.011;a2+=0.007;a3+=0.005;
  ctx.fillStyle='rgba(0,0,0,0.18)';ctx.fillRect(0,0,W,H);
  const sc=Math.min(W,H)*0.18;
  const pts=verts4.map(v=>{const[x,y,z,w]=rot4(v);const f4=2/(w+2.5);const f3=W*0.35/(z*f4+2.5);return[W/2+x*f4*f3,H/2+y*f4*f3,z*f4];});
  edges4.forEach(([a,b],i)=>{
    const[x1,y1,z1]=pts[a],[x2,y2,z2]=pts[b];
    const hue=(i*11+t*30)%360;
    ctx.strokeStyle=`hsla(${hue|0},100%,65%,0.75)`;ctx.lineWidth=1.5;
    ctx.beginPath();ctx.moveTo(x1,y1);ctx.lineTo(x2,y2);ctx.stroke();
  });
  pts.forEach(([x,y])=>{ctx.fillStyle='rgba(255,255,255,0.7)';ctx.beginPath();ctx.arc(x,y,2,0,Math.PI*2);ctx.fill();});
  requestAnimationFrame(draw);
}
draw();

</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char RETROGEO_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>RETRO GEO · COSMIC-S3</title><style>*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);border-bottom:1px solid #ff44ff;padding:5px 10px;display:flex;justify-content:space-between;font-size:11px;z-index:9}.nav a{color:#ff44ff;text-decoration:none}
canvas{display:block}</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>RETRO GEO</span></div><canvas id="c"></canvas>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
let W,H;function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();if(typeof init==='function')init();});
resize();

const PALETTE=['#ff00ff','#00ffff','#ffff00','#ff8800','#00ff44','#ff0088','#8800ff','#44ffff'];
const shapes=[];
function spawn(){
  const type=['circle','rect','triangle','line'][Math.floor(Math.random()*4)];
  const color=PALETTE[Math.floor(Math.random()*PALETTE.length)];
  const sz=20+Math.random()*80;
  shapes.push({type,color,x:Math.random()*W,y:Math.random()*H,sz,
    vx:(Math.random()-0.5)*2,vy:(Math.random()-0.5)*2,
    rot:Math.random()*Math.PI*2,spin:(Math.random()-0.5)*0.04,life:1});
}
for(let i=0;i<20;i++)spawn();
function drawShape(s){
  ctx.save();ctx.translate(s.x,s.y);ctx.rotate(s.rot);
  ctx.strokeStyle=s.color;ctx.lineWidth=2;ctx.globalAlpha=s.life;
  if(s.type==='circle'){ctx.beginPath();ctx.arc(0,0,s.sz/2,0,Math.PI*2);ctx.stroke();}
  else if(s.type==='rect'){ctx.strokeRect(-s.sz/2,-s.sz/2,s.sz,s.sz);}
  else if(s.type==='triangle'){ctx.beginPath();ctx.moveTo(0,-s.sz/2);ctx.lineTo(s.sz/2,s.sz/2);ctx.lineTo(-s.sz/2,s.sz/2);ctx.closePath();ctx.stroke();}
  else{ctx.beginPath();ctx.moveTo(-s.sz/2,0);ctx.lineTo(s.sz/2,0);ctx.stroke();}
  ctx.globalAlpha=1;ctx.restore();
}
let t=0;
function draw(){
  t++;ctx.fillStyle='rgba(0,0,0,0.15)';ctx.fillRect(0,0,W,H);
  // scanlines
  if(t%4===0){ctx.fillStyle='rgba(0,0,0,0.05)';for(let y=0;y<H;y+=4)ctx.fillRect(0,y,W,1);}
  for(let i=shapes.length-1;i>=0;i--){
    const s=shapes[i];
    s.x+=s.vx;s.y+=s.vy;s.rot+=s.spin;s.life-=0.003;
    if(s.life<=0){shapes.splice(i,1);spawn();continue;}
    if(s.x<-100||s.x>W+100||s.y<-100||s.y>H+100){s.x=Math.random()*W;s.y=Math.random()*H;}
    drawShape(s);
  }
  requestAnimationFrame(draw);
}
draw();

</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char MIRRORBLOB_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>MIRROR BLOB · COSMIC-S3</title><style>*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);border-bottom:1px solid #ff44aa;padding:5px 10px;display:flex;justify-content:space-between;font-size:11px;z-index:9}.nav a{color:#ff44aa;text-decoration:none}
canvas{display:block}</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>MIRROR BLOB</span></div><canvas id="c"></canvas>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
let W,H;function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();if(typeof init==='function')init();});
resize();

let t=0;
function draw(){
  t+=0.025;
  ctx.fillStyle='rgba(0,0,0,0.18)';ctx.fillRect(0,0,W,H);
  const qw=W/2,qh=H/2;
  // draw blob in top-left quadrant, mirror to others
  for(let m=0;m<4;m++){
    ctx.save();
    ctx.translate(m%2===0?0:W,m<2?0:H);
    ctx.scale(m%2===0?1:-1,m<2?1:-1);
    // blob path
    const N=6,pts=[];
    for(let i=0;i<N;i++){
      const base=i/N*Math.PI*2;
      const r=qw*0.35+qw*0.12*Math.sin(t*2+i*1.3)+qw*0.08*Math.sin(t*3.1+i*2.7);
      pts.push([qw/2+Math.cos(base)*r,qh/2+Math.sin(base)*r]);
    }
    const hue=(t*20)%360;
    ctx.beginPath();
    ctx.moveTo(pts[0][0],pts[0][1]);
    for(let i=0;i<N;i++){
      const p=pts[(i+1)%N],pp=pts[i];
      const mx=(pp[0]+p[0])/2,my=(pp[1]+p[1])/2;
      ctx.quadraticCurveTo(pp[0],pp[1],mx,my);
    }
    ctx.closePath();
    const g=ctx.createRadialGradient(qw/2,qh/2,0,qw/2,qh/2,qw*0.45);
    g.addColorStop(0,`hsla(${hue|0},100%,75%,0.8)`);
    g.addColorStop(0.5,`hsla(${(hue+40)|0},100%,55%,0.6)`);
    g.addColorStop(1,`hsla(${(hue+80)|0},100%,30%,0.2)`);
    ctx.fillStyle=g;ctx.fill();
    ctx.strokeStyle=`hsla(${hue|0},100%,85%,0.5)`;ctx.lineWidth=2;ctx.stroke();
    ctx.restore();
  }
  // center cross glow
  const cg=ctx.createRadialGradient(W/2,H/2,0,W/2,H/2,30);
  cg.addColorStop(0,'rgba(255,255,255,0.5)');cg.addColorStop(1,'transparent');
  ctx.fillStyle=cg;ctx.beginPath();ctx.arc(W/2,H/2,30,0,Math.PI*2);ctx.fill();
  requestAnimationFrame(draw);
}
draw();

</script></body></html>
)EOF";

// ---------------------------------------------------------------------------
static const char CITYFLOW_HTML[] = R"EOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>CITY FLOW · COSMIC-S3</title><style>*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;overflow:hidden;font-family:monospace}
.nav{position:fixed;top:0;left:0;right:0;background:rgba(0,0,0,.7);border-bottom:1px solid #ffaa44;padding:5px 10px;display:flex;justify-content:space-between;font-size:11px;z-index:9}.nav a{color:#ffaa44;text-decoration:none}
canvas{display:block}</style>
</head><body>
<div class="nav"><a href="/">&#x2B21; MODES</a><span>CITY FLOW</span></div><canvas id="c"></canvas>
<script>
const C=document.getElementById('c'),ctx=C.getContext('2d');
let W,H;function resize(){W=C.width=innerWidth;H=C.height=innerHeight;}
window.addEventListener('resize',()=>{resize();if(typeof init==='function')init();});
resize();

const ROWS=12,COLS=16,NCARS=80;
let gridW,gridH,cw,ch;
const cars=[];
function init(){
  gridW=W;gridH=H;cw=W/COLS;ch=H/ROWS;
  cars.length=0;
  for(let i=0;i<NCARS;i++){
    const horiz=Math.random()<0.5;
    const lane=Math.floor(Math.random()*(horiz?ROWS:COLS));
    const pos=Math.random()*(horiz?COLS:ROWS);
    const dir=Math.random()<0.5?1:-1;
    const hue=horiz?200+Math.random()*40:20+Math.random()*40;
    cars.push({horiz,lane,pos,dir,speed:(0.04+Math.random()*0.04)*dir,hue});
  }
}
let t=0;
function draw(){
  t++;ctx.fillStyle='#050505';ctx.fillRect(0,0,W,H);
  // road grid
  ctx.strokeStyle='rgba(50,50,80,0.8)';ctx.lineWidth=1;
  for(let r=0;r<=ROWS;r++){ctx.beginPath();ctx.moveTo(0,r*ch);ctx.lineTo(W,r*ch);ctx.stroke();}
  for(let c=0;c<=COLS;c++){ctx.beginPath();ctx.moveTo(c*cw,0);ctx.lineTo(c*cw,H);ctx.stroke();}
  // buildings in cells
  for(let r=0;r<ROWS;r++)for(let c=0;c<COLS;c++){
    const bright=30+((r*3+c*7)%30);
    ctx.fillStyle=`rgb(${bright},${bright},${bright+10})`;
    ctx.fillRect(c*cw+3,r*ch+3,cw-6,ch-6);
  }
  // cars (headlights + taillights trail)
  cars.forEach(car=>{
    car.pos+=car.speed;
    const max=car.horiz?COLS:ROWS;
    if(car.pos>max+1)car.pos=-1;if(car.pos<-1)car.pos=max+1;
    const x=car.horiz?car.pos*cw:car.lane*cw+cw/2;
    const y=car.horiz?car.lane*ch+ch/2:car.pos*ch;
    // trail
    ctx.fillStyle=`hsla(${car.dir>0?car.hue:(car.hue+180)%360},100%,50%,0.25)`;
    for(let i=1;i<=4;i++){
      const tx=car.horiz?x-car.dir*i*6:x,ty=car.horiz?y:y-car.dir*i*6;
      ctx.beginPath();ctx.arc(tx,ty,2,0,Math.PI*2);ctx.fill();
    }
    // headlight
    ctx.fillStyle=`hsla(${car.hue},100%,80%,0.9)`;
    ctx.beginPath();ctx.arc(x,y,3,0,Math.PI*2);ctx.fill();
    const glow=ctx.createRadialGradient(x,y,0,x,y,10);
    glow.addColorStop(0,`hsla(${car.hue},100%,70%,0.4)`);glow.addColorStop(1,'transparent');
    ctx.fillStyle=glow;ctx.beginPath();ctx.arc(x,y,10,0,Math.PI*2);ctx.fill();
  });
  requestAnimationFrame(draw);
}
init();draw();

</script></body></html>
)EOF";

// ---------------------------------------------------------------------------


// ═══════════════════════════════════════════════════════════════════════════
//  HTTP Handler Functions
// ═══════════════════════════════════════════════════════════════════════════


void handleRedirect() {
    cpServer->sendHeader("Location", String("http://") + CP_PORTAL_IP "/", true);
    cpServer->send(302, "text/plain", "");
}

void handleIndex()      { cpServer->send(200, "text/html", INDEX_HTML);       }
void handleSafety()     { cpServer->send(200, "text/html", SAFETY_HTML);      }
void handleMandala()    { cpServer->send(200, "text/html", MANDALA_HTML);     }
void handlePlasma()     { cpServer->send(200, "text/html", PLASMA_HTML);      }
void handleFractal()    { cpServer->send(200, "text/html", FRACTAL_HTML);     }
void handleMatrix()     { cpServer->send(200, "text/html", MATRIX_HTML);      }
void handleCyber()      { cpServer->send(200, "text/html", CYBER_HTML);       }
void handleBinary()     { cpServer->send(200, "text/html", BINARY_HTML);      }
void handleStarfield()  { cpServer->send(200, "text/html", STARFIELD_HTML);   }
void handleParticles()  { cpServer->send(200, "text/html", PARTICLES_HTML);   }
void handleTunnel()     { cpServer->send(200, "text/html", TUNNEL_HTML);      }
void handleMfire()      { cpServer->send(200, "text/html", MFIRE_HTML);       }
void handleMice()       { cpServer->send(200, "text/html", MICE_HTML);        }
void handleMstorm()     { cpServer->send(200, "text/html", MSTORM_HTML);      }
void handleMblood()     { cpServer->send(200, "text/html", MBLOOD_HTML);      }
void handleMgold()      { cpServer->send(200, "text/html", MGOLD_HTML);       }
void handleMvoid()      { cpServer->send(200, "text/html", MVOID_HTML);       }
void handleMphantom()   { cpServer->send(200, "text/html", MPHANTOM_HTML);    }
void handleMripple()    { cpServer->send(200, "text/html", MRIPPLE_HTML);     }
void handleMglitch()    { cpServer->send(200, "text/html", MGLITCH_HTML);     }
void handleHopalong()   { cpServer->send(200, "text/html", HOPALONG_HTML);    }
void handleInterference(){ cpServer->send(200, "text/html", INTERFERENCE_HTML); }
void handleVoronoi()    { cpServer->send(200, "text/html", VORONOI_HTML);     }
void handleStrange()    { cpServer->send(200, "text/html", STRANGE_HTML);     }
void handleLissajous()  { cpServer->send(200, "text/html", LISSAJOUS_HTML);   }
void handleSierpinski() { cpServer->send(200, "text/html", SIERPINSKI_HTML);  }
void handleSpirograph() { cpServer->send(200, "text/html", SPIROGRAPH_HTML);  }
void handleBarnsley()   { cpServer->send(200, "text/html", BARNSLEY_HTML);    }
void handleCampfire()   { cpServer->send(200, "text/html", CAMPFIRE_HTML);    }
void handleRaindrops()  { cpServer->send(200, "text/html", RAINDROPS_HTML);   }
void handleGameoflife() { cpServer->send(200, "text/html", GAMEOFLIFE_HTML);  }
void handleAurora()     { cpServer->send(200, "text/html", AURORA_HTML);      }
void handleKaleidoscope(){ cpServer->send(200, "text/html", KALEIDOSCOPE_HTML); }
void handleDragon()     { cpServer->send(200, "text/html", DRAGON_HTML);      }
void handleLava2()      { cpServer->send(200, "text/html", LAVA2_HTML);       }
void handleNoise()      { cpServer->send(200, "text/html", NOISE_HTML);       }
void handleSnake()      { cpServer->send(200, "text/html", SNAKE_HTML);       }
void handleBreakout()   { cpServer->send(200, "text/html", BREAKOUT_HTML);    }
void handleTetris()     { cpServer->send(200, "text/html", TETRIS_HTML);      }
void handleDodge()      { cpServer->send(200, "text/html", DODGE_HTML);       }
void handleAsteroids()  { cpServer->send(200, "text/html", ASTEROIDS_HTML);   }
void handleApollonian() { cpServer->send(200, "text/html", APOLLONIAN_HTML);  }
void handleSunflower()  { cpServer->send(200, "text/html", SUNFLOWER_HTML);   }
void handleQuasicrystal(){ cpServer->send(200, "text/html", QUASICRYSTAL_HTML); }
void handleLorenz()     { cpServer->send(200, "text/html", LORENZ_HTML);      }
void handleMandelbrot() { cpServer->send(200, "text/html", MANDELBROT_HTML);  }
void handleReaction()   { cpServer->send(200, "text/html", REACTION_HTML);    }
void handleMaze()       { cpServer->send(200, "text/html", MAZE_HTML);        }
void handleVines()      { cpServer->send(200, "text/html", VINES_HTML);       }
void handleSnowflakes() { cpServer->send(200, "text/html", SNOWFLAKES_HTML);  }
void handleCube3d()     { cpServer->send(200, "text/html", CUBE3D_HTML);      }
void handleTorus()      { cpServer->send(200, "text/html", TORUS_HTML);       }
void handleHypercube()  { cpServer->send(200, "text/html", HYPERCUBE_HTML);   }
void handleRetrogeo()   { cpServer->send(200, "text/html", RETROGEO_HTML);    }
void handleMirrorblob() { cpServer->send(200, "text/html", MIRRORBLOB_HTML);  }
void handleCityflow()   { cpServer->send(200, "text/html", CITYFLOW_HTML);    }
void handleFireworks()  { cpServer->send(200, "text/html", FIREWORKS_HTML);   }
void handleCoral()      { cpServer->send(200, "text/html", CORAL_HTML);       }
void handleCwaves()     { cpServer->send(200, "text/html", CWAVES_HTML);      }
void handleDeepstars()  { cpServer->send(200, "text/html", DEEPSTARS_HTML);   }
void handleFlowfield()  { cpServer->send(200, "text/html", FLOWFIELD_HTML);   }
void handleMetaballs()  { cpServer->send(200, "text/html", METABALLS_HTML);   }
void handleGoop()       { cpServer->send(200, "text/html", GOOP_HTML);        }
void handleWormhole()   { cpServer->send(200, "text/html", WORMHOLE_HTML);    }
void handleCrystal()    { cpServer->send(200, "text/html", CRYSTAL_HTML);     }
void handleLightning()  { cpServer->send(200, "text/html", LIGHTNING_HTML);   }
void handleBounceballs(){ cpServer->send(200, "text/html", BOUNCEBALLS_HTML); }
void handleNeonrain()   { cpServer->send(200, "text/html", NEONRAIN_HTML);    }
void handleDna()        { cpServer->send(200, "text/html", DNA_HTML);         }
void handleSandfall()   { cpServer->send(200, "text/html", SANDFALL_HTML);    }
void handleAcidspiral() { cpServer->send(200, "text/html", ACIDSPIRAL_HTML);  }
void handlePlasmaglobe(){ cpServer->send(200, "text/html", PLASMAGLOBE_HTML); }
void handleWarpgrid()   { cpServer->send(200, "text/html", WARPGRID_HTML);    }
void handleNebula()     { cpServer->send(200, "text/html", NEBULA_HTML);      }

// ── /api/msg ──────────────────────────────────────────────────────────────────
void handleApiMsg() {
    String json = "{\"id\":" + String(cpMsgId) + ",\"msg\":\"" + cpPendingMsg + "\"}";
    cpServer->send(200, "application/json", json);
}

// ── /api/visitor-msg ────────────────────────────────────────────────────────

// ═══════════════════════════════════════════════════════════════════════════
//  SETAP CSS
// ═══════════════════════════════════════════════════════════════════════════



// ═══════════════════════════════════════════════════════════════════════════
static const char SETAP_CSS[] =
    "*{margin:0;padding:0;box-sizing:border-box}"
    "body{background:radial-gradient(ellipse at 50% 50%,#0d003d,#000010 70%);"
    "min-height:100vh;font-family:'Courier New',monospace;color:#fff;padding:20px}"
    "h1{font-size:1.1rem;letter-spacing:6px;text-align:center;margin-bottom:4px;"
    "background:linear-gradient(90deg,#c77dff,#8338ec);-webkit-background-clip:text;"
    "-webkit-text-fill-color:transparent}"
    ".sub{font-size:.4rem;letter-spacing:5px;color:rgba(199,119,255,.35);"
    "text-align:center;margin-bottom:20px}"
    "nav{display:flex;justify-content:space-between;margin-bottom:18px;font-size:.5rem;"
    "letter-spacing:2px}nav a{color:rgba(199,119,255,.7);text-decoration:none}"
    ".box{background:rgba(131,56,236,.1);border:1px solid rgba(131,56,236,.4);"
    "border-radius:10px;padding:20px}"
    "label{display:block;font-size:.45rem;letter-spacing:3px;color:rgba(199,119,255,.6);"
    "margin-bottom:6px}"
    "input[type=text]{width:100%;padding:10px;background:rgba(0,0,0,.5);"
    "border:1px solid rgba(131,56,236,.5);border-radius:6px;color:#fff;"
    "font-family:'Courier New',monospace;font-size:.6rem;margin-bottom:14px}"
    ".emojis{display:flex;flex-wrap:wrap;gap:8px;margin-bottom:16px}"
    ".emojis button{background:rgba(131,56,236,.15);border:1px solid rgba(131,56,236,.4);"
    "border-radius:6px;padding:6px 10px;font-size:1.2rem;cursor:pointer;color:#fff}"
    ".emojis button:active{background:rgba(131,56,236,.5)}"
    ".row{display:flex;gap:8px}"
    ".btn{flex:1;padding:12px;border:none;border-radius:8px;font-family:'Courier New',monospace;"
    "font-size:.5rem;letter-spacing:3px;cursor:pointer}"
    ".save{background:linear-gradient(135deg,#8338ec,#c77dff);color:#fff}"
    ".reset{background:rgba(255,80,80,.15);border:1px solid rgba(255,80,80,.4);color:#f88}"
    ".note{font-size:.38rem;letter-spacing:2px;color:rgba(199,119,255,.4);margin-top:12px;"
    "text-align:center}";


//  Cosmic Wiki
// ═══════════════════════════════════════════════════════════════════════════




struct WikiArticle { const char* title; uint8_t cat; const char* body; };
static const char* const WIKI_CATS[] = {
    "\xF0\x9F\x8C\x8C Space &amp; Cosmology",
    "\xE2\x9A\x9B\xEF\xB8\x8F Quantum Physics",
    "\xF0\x9F\xA7\xA0 Consciousness",
    "\xF0\x9F\x8C\xBF Ethnobotanicals",
    "\xF0\x9F\x91\x81\xEF\xB8\x8F Strange Phenomena",
    "\xF0\x9F\x8C\x80 Cosmic Philosophy",
    "\xF0\x9F\x8F\xBA Human Curiosities",
    "\xF0\x9F\x94\xAC Science &amp; Tech",
    "\xF0\x9F\x94\xAE Occult &amp; Esoteric",
    "\xF0\x9F\xA7\xAC Weird Biology"
};
static const uint8_t WIKI_CAT_COUNT = 10;
static const uint8_t WIKI_CAT_SIZES[] = {6, 6, 5, 5, 5, 5, 5, 5, 5, 5};

// ── Article bodies in flash ────────────────────────────────────────────────────

static const char WA0[] =
    "<p><b>Black holes</b> are regions of spacetime where gravity is so extreme that nothing &mdash; not even light &mdash; "
    "can escape once it crosses the <b>event horizon</b>. They form when massive stars exhaust their nuclear fuel and collapse "
    "under their own gravity, compressing matter to a <b>singularity</b> of theoretically infinite density at the center.</p>"
    "<p>There are several classes: <b>stellar black holes</b> (a few to tens of solar masses) formed by collapsing stars; "
    "<b>supermassive black holes</b> (millions to billions of solar masses) lurking at galactic centers including our own "
    "Milky Way&#39;s <b>Sagittarius A*</b>; and hypothetical <b>primordial black holes</b> that may have formed in the "
    "early universe.</p>"
    "<p>In 1974, <b>Stephen Hawking</b> showed that black holes are not entirely black &mdash; quantum effects near the "
    "event horizon cause them to slowly radiate thermal energy, eventually evaporating entirely. This raised the "
    "<b>information paradox</b>: does information about infalling matter get destroyed, violating quantum mechanics? "
    "The resolution remains one of physics&#39; deepest open questions.</p>"
    "<p>The <b>first direct image</b> of a black hole was captured in 2019 by the Event Horizon Telescope: a glowing ring "
    "of superheated plasma around the shadow of M87*, 6.5 billion solar masses, 55 million light-years away. In 2022, "
    "Sagittarius A* was imaged too. These images confirmed Einstein&#39;s general relativity in the most extreme environment "
    "the cosmos offers &mdash; a triumph of collective human ingenuity spanning continents and decades.</p>";

static const char WA1[] =
    "<p><b>Dark matter</b> is an invisible, non-luminous substance comprising roughly <b>27% of the universe&#39;s total "
    "energy content</b>, yet emitting, absorbing, or reflecting no light. We infer its existence purely from gravitational "
    "effects: galaxies rotate far too fast for their visible mass alone, and light bends around galaxy clusters far more "
    "than visible matter can explain.</p>"
    "<p>Fritz Zwicky first proposed dark matter in 1933 observing the Coma Cluster. Vera Rubin&#39;s landmark work in the "
    "1970s on <b>galaxy rotation curves</b> made the case undeniable. The leading candidate is the <b>WIMP</b> (Weakly "
    "Interacting Massive Particle), though decades of underground detectors have found nothing. Axions and sterile neutrinos "
    "are other contenders.</p>"
    "<p><b>Dark energy</b> is stranger still: a mysterious repulsive force comprising roughly <b>68% of the cosmos</b>, "
    "driving the accelerating expansion discovered in 1998 by Perlmutter and Schmidt (Nobel 2011). The simplest explanation "
    "is Einstein&#39;s <b>cosmological constant</b> &Lambda; &mdash; a fixed energy density of empty space &mdash; but its "
    "measured value is 120 orders of magnitude smaller than quantum field theory predicts: the <b>cosmological constant "
    "problem</b>.</p>"
    "<p>Together, dark matter and dark energy constitute <b>95% of everything</b>. The ordinary atoms comprising stars, "
    "planets, and people are a 5% cosmic footnote. The universe&#39;s dominant ingredients remain completely unknown, "
    "making cosmology&#39;s deepest challenge also its most humbling.</p>";

static const char WA2[] =
    "<p><b>Neutron stars</b> are the collapsed cores left after a massive star dies in a supernova. Despite having a mass "
    "1.4&ndash;2 times our Sun, they are crushed into a sphere just 20 kilometres across &mdash; the size of a city. A "
    "single teaspoon of neutron star material would weigh roughly <b>one billion tons</b>.</p>"
    "<p>Inside, electrons and protons are crushed together into <b>neutrons</b> by extreme gravity. The outer layers form "
    "a rigid crystalline crust; deeper in, neutrons may flow as a frictionless superfluid. At the very core, densities may "
    "exceed anything achievable in a lab, possibly producing exotic <b>quark-gluon plasma</b> or strange quark matter.</p>"
    "<p>When a neutron star rotates rapidly and emits beams of electromagnetic radiation from its magnetic poles, it is "
    "observed as a <b>pulsar</b>. The first was discovered in 1967 by Jocelyn Bell Burnell, initially nicknamed LGM-1 "
    "(Little Green Men). Millisecond pulsars spin hundreds of times per second with clockwork regularity surpassing atomic "
    "clocks &mdash; invaluable tools for detecting <b>gravitational waves</b> and testing general relativity.</p>"
    "<p><b>Magnetars</b> are neutron stars with magnetic fields a trillion times stronger than Earth&#39;s, the most "
    "intense in the known universe. They occasionally release <b>starquakes</b> &mdash; crust-crack events emitting more "
    "energy in a fraction of a second than our Sun emits in 100,000 years. In 2004, a magnetar flare from 50,000 "
    "light-years away briefly ionized Earth&#39;s upper atmosphere, demonstrating the cosmos&#39; capacity for violence "
    "at unimaginable scales.</p>";

static const char WA3[] =
    "<p>The <b>Cosmic Microwave Background</b> (CMB) is the faint thermal afterglow of the <b>Big Bang</b> itself &mdash; "
    "the oldest light in the universe, dating to just 380,000 years after the cosmos was born. Before this moment the "
    "universe was a dense, opaque plasma too hot for atoms to form. When it cooled enough for electrons and protons to "
    "combine into hydrogen, the universe became transparent and light streamed freely for the first time.</p>"
    "<p>This primordial light has been stretched by 13.8 billion years of cosmic expansion into the <b>microwave "
    "spectrum</b>, arriving from all directions at just <b>2.725 Kelvin</b>. It was accidentally discovered in 1964 by "
    "Arno Penzias and Robert Wilson at Bell Labs, who initially blamed the hiss in their antenna on pigeon droppings.</p>"
    "<p>The CMB contains tiny <b>temperature fluctuations</b> of one part in 100,000 that encode the seeds of all cosmic "
    "structure. Dense regions became galaxy clusters; voids became vast emptiness. <b>COBE</b> first mapped these in 1992, "
    "WMAP refined them, and the <b>Planck satellite</b> produced the definitive map in 2013.</p>"
    "<p>Reading the CMB like a cosmic blueprint, physicists determined the universe is <b>13.8 billion years old</b>, "
    "spatially flat to within 0.4%, and composed of 5% ordinary matter, 27% dark matter, and 68% dark energy. No other "
    "measurement in science has revealed more about our origins from a single source of data.</p>";

static const char WA4[] =
    "<p>In 1950, physicist <b>Enrico Fermi</b> posed a deceptively simple question: given the age and size of the universe "
    "and the seemingly high probability that other intelligent civilizations should exist, <b>where is everybody?</b> This "
    "became the Fermi Paradox &mdash; the stark contradiction between high estimates for extraterrestrial intelligence "
    "and the complete absence of any evidence for it.</p>"
    "<p>The <b>Drake Equation</b> (1961) estimates communicating civilizations by multiplying factors like star formation "
    "rate, fraction with planets, fraction with life, and so on. Depending on assumptions, answers range from millions of "
    "civilizations to fewer than one. The silence of the cosmos is deafening either way.</p>"
    "<p>Proposed resolutions span the mundane to the terrifying. The <b>Rare Earth hypothesis</b> suggests complex life "
    "requires an unlikely combination of factors. The <b>Great Filter</b>, proposed by Robin Hanson, suggests a "
    "catastrophic barrier lies either in our past (we got lucky) or our future (civilizations routinely destroy "
    "themselves). The <b>Zoo hypothesis</b> proposes advanced civilizations avoid contact. <b>Dark Forest theory</b> "
    "(Liu Cixin) suggests silence is a survival strategy in a predatory cosmos.</p>"
    "<p>The detection of anomalous fast radio bursts, the dimming of <b>Tabby&#39;s Star</b>, and official acknowledgment "
    "of <b>UAP phenomena</b> have reinvigorated interest. SETI&#39;s ongoing surveys have found nothing conclusive. "
    "Whether the silence reflects rarity of life, brevity of civilizations, or something stranger remains one of the most "
    "profound unanswered questions humanity has ever asked.</p>";

static const char WA5[] =
    "<p>The <b>observable universe</b> is the spherical region of space from which light has had time to reach us in the "
    "13.8 billion years since the Big Bang. Because the universe has been expanding &mdash; and accelerating &mdash; the "
    "actual radius is approximately <b>46.5 billion light-years</b>, larger than the naive 13.8 billion figure.</p>"
    "<p>The observable universe contains roughly <b>two trillion galaxies</b>, each harboring hundreds of billions of "
    "stars. The <b>cosmic web</b> organizes galaxies into filaments and sheets surrounding vast empty voids, a structure "
    "seeded by quantum fluctuations in the early universe magnified by inflation. Our Milky Way is part of the Local Group, "
    "the Virgo Supercluster, and the larger <b>Laniakea Supercluster</b> of 100,000 galaxies spanning 520 million "
    "light-years.</p>"
    "<p>Beyond our horizon lies the <b>unobservable universe</b>, possibly infinite. Regions beyond our <b>Hubble "
    "sphere</b> recede faster than light due to space itself expanding &mdash; not violating relativity since it is space "
    "that expands, not objects moving through space. We can never receive information from or send signals to these "
    "regions.</p>"
    "<p>The edge of the observable universe is not a wall but a <b>cosmic horizon</b>, different for every observer. An "
    "alien in a galaxy 10 billion light-years away has a completely different observable universe, overlapping with ours "
    "only partially. We each inhabit our own private bubble of cosmic visibility &mdash; islands of knowability in an "
    "ocean of the forever unreachable.</p>";

static const char WA6[] =
    "<p>One of quantum mechanics&#39; most startling discoveries is that every particle of matter and every photon of "
    "light behaves as both a <b>wave</b> and a <b>particle</b> &mdash; not one or the other, but both simultaneously, "
    "with the observed behavior depending entirely on how you choose to measure it. This is <b>wave-particle duality</b>, "
    "and it defies every classical intuition about the nature of reality.</p>"
    "<p>The classic demonstration is the <b>double-slit experiment</b>: fire electrons at a barrier with two slits and an "
    "interference pattern appears on the screen beyond &mdash; the signature of waves. But install a detector to record "
    "which slit each particle passes through and the interference pattern vanishes, replaced by two bands as if particles. "
    "The act of <b>measurement itself</b> changes the outcome.</p>"
    "<p>Louis de Broglie proposed in 1924 that all matter has an associated wavelength: &lambda; = h/mv, where h is "
    "Planck&#39;s constant and mv is momentum. This was confirmed by electron diffraction. The wavelength of macroscopic "
    "objects is vanishingly small, which is why your body doesn&#39;t diffract around doorframes. But at atomic scales, "
    "wave behavior dominates everything.</p>"
    "<p>The <b>complementarity principle</b>, formulated by Niels Bohr, holds that wave and particle aspects are "
    "<b>mutually exclusive but equally real</b> descriptions. Attempts to observe both simultaneously always fail. This "
    "is not a limitation of instruments but an irreducible feature of nature itself &mdash; the universe seems to enforce "
    "a fundamental limit on how much reality can be pinned down at once.</p>";

static const char WA7[] =
    "<p><b>Quantum entanglement</b> occurs when two or more particles become correlated such that the quantum state of "
    "each cannot be described independently of the others, no matter how far apart they are. Measuring a property of one "
    "particle instantly determines the corresponding property of the other &mdash; Einstein called this "
    "<b>&ldquo;spooky action at a distance&rdquo;</b> and refused to believe it was real.</p>"
    "<p>Einstein, Podolsky, and Rosen argued in 1935 that entanglement implied either non-locality or that quantum "
    "mechanics was incomplete &mdash; that <b>hidden variables</b> determined outcomes in advance. In 1964, John Bell "
    "derived a mathematical inequality any hidden-variable theory must satisfy. Experiments by Alain Aspect in 1982, and "
    "loophole-free tests in 2015, showed nature <b>violates Bell&#39;s inequalities</b>: reality is genuinely non-local.</p>"
    "<p>Entanglement does not allow faster-than-light communication. Correlations are only apparent after comparing "
    "measurements through classical channels. However, entanglement powers <b>quantum teleportation</b> (transmitting "
    "quantum states), <b>quantum cryptography</b> (provably secure communication), and <b>quantum computing</b> "
    "(exponential speedups for certain problems).</p>"
    "<p>The deepest implication is that <b>separability is an illusion</b>. Once particles interact, they remain "
    "connected across arbitrary distances. Some physicists speculate that entanglement of particles in the early universe "
    "is woven into spacetime itself &mdash; that space, geometry, and gravity may <b>emerge from entanglement</b> rather "
    "than being fundamental.</p>";

static const char WA8[] =
    "<p>In 1927, Werner Heisenberg showed there is a fundamental limit to how precisely we can simultaneously know certain "
    "pairs of physical properties. The famous formulation: the more precisely we know a particle&#39;s <b>position</b>, "
    "the less precisely we can know its <b>momentum</b>, and vice versa: &Delta;x &middot; &Delta;p &ge; &hbar;/2. This "
    "is the <b>Heisenberg Uncertainty Principle</b>.</p>"
    "<p>Crucially, this is <b>not a limitation of instruments</b>. It is not that measuring position disturbs momentum in "
    "some avoidable way. The uncertainty is irreducible and ontological &mdash; a particle genuinely does not have a "
    "precise position and momentum simultaneously. These are not hidden properties waiting to be measured; they are "
    "undefined until measured.</p>"
    "<p>A complementary relation exists between <b>energy and time</b>: &Delta;E &middot; &Delta;t &ge; &hbar;/2. This "
    "allows <b>virtual particles</b> &mdash; quantum fluctuations that briefly borrow energy from the vacuum &mdash; to "
    "exist transiently. The <b>Casimir effect</b> and the <b>Lamb shift</b> in hydrogen spectra both arise from vacuum "
    "fluctuations and have been measured to extraordinary precision.</p>"
    "<p>The uncertainty principle explains why atoms don&#39;t collapse (confinement raises kinetic energy), why "
    "<b>white dwarfs</b> don&#39;t implode (electron degeneracy pressure), why <b>quantum tunneling</b> is possible, and "
    "why zero-point energy is real. Reality is irreducibly fuzzy at its smallest scales &mdash; and this fuzziness is the "
    "engine of chemistry, nuclear physics, and life itself.</p>";

static const char WA9[] =
    "<p>In 1957, Hugh Everett III proposed a radical solution to quantum mechanics&#39; measurement problem. Rather than "
    "wavefunction collapse &mdash; the mysterious process by which a quantum superposition becomes a definite outcome "
    "&mdash; Everett argued that <b>nothing ever collapses</b>. Every possible outcome of every quantum event actually "
    "occurs, each in its own <b>branch of a vast, ever-splitting universal wavefunction</b>.</p>"
    "<p>In the <b>Many-Worlds Interpretation</b>, when Schr&ouml;dinger&#39;s cat is measured, the universe does not "
    "randomly choose alive or dead. Instead it branches: in one branch the cat lives, in another it dies, and both are "
    "equally real. The observer also splits, each copy experiencing a single definite outcome &mdash; which is why we "
    "never perceive superpositions directly.</p>"
    "<p>The branching follows from <b>decoherence</b>: quantum systems interacting with their environment develop "
    "correlations that make interference between branches vanishingly small, effectively hiding all but one branch from "
    "each observer. MWI is arguably the most minimal interpretation &mdash; it adds nothing to the formalism, only takes "
    "the mathematics seriously.</p>"
    "<p>The philosophical cost is staggering: an <b>uncountably infinite</b> number of parallel universes, constantly "
    "multiplying. There is a version of you who made every different choice at every moment. Proponents argue this is "
    "worthwhile for a deterministic, collapse-free theory. Critics note that deriving the <b>Born rule</b> (why "
    "probabilities are squared amplitudes) from MWI without circular reasoning remains genuinely unsolved.</p>";

static const char WA10[] =
    "<p><b>Quantum tunneling</b> is the phenomenon where a particle passes through an energy barrier that it classically "
    "could not surmount. In classical physics, if a ball lacks the energy to roll over a hill, it bounces back. In quantum "
    "mechanics, a particle&#39;s wavefunction extends through the barrier, giving it a finite probability of appearing on "
    "the other side &mdash; as if it <b>tunneled through</b> the hill entirely.</p>"
    "<p>The effect is real and pervasive. <b>Alpha decay</b> in radioactive nuclei occurs because alpha particles tunnel "
    "through the nuclear potential barrier. Without tunneling, the Sun could not shine: proton-proton fusion requires "
    "overcoming electromagnetic repulsion at temperatures far below the classical threshold &mdash; quantum tunneling "
    "bridges the gap, enabling <b>stellar nucleosynthesis</b> and making life possible.</p>"
    "<p>Technology exploits tunneling extensively. The <b>tunnel diode</b> (1957) achieves switching speeds impossible "
    "with classical transistors. The <b>scanning tunneling microscope</b> (Nobel 1986) maps individual atoms by measuring "
    "the tunneling current between a sharp tip and a surface, achieving sub-angstrom resolution. <b>Flash memory</b> "
    "&mdash; the basis of all solid-state storage &mdash; stores bits by moving electrons through insulators via "
    "tunneling.</p>"
    "<p>Biological implications may run deep. Enzyme catalysis may exploit proton tunneling to accelerate reactions far "
    "beyond what thermal activation alone explains. <b>DNA mutation rates</b> may be influenced by proton tunneling in "
    "hydrogen bonds. If quantum tunneling threads through the biochemistry of life, then quantum weirdness is not merely "
    "a laboratory curiosity &mdash; it is woven into every living cell.</p>";

static const char WA11[] =
    "<p>In 1935, Erwin Schr&ouml;dinger devised a thought experiment to expose what he saw as an absurdity in the "
    "<b>Copenhagen interpretation</b>. A cat is placed in a sealed box with a radioactive atom, a Geiger counter, and a "
    "vial of poison: if the atom decays, the counter triggers a mechanism killing the cat. Quantum mechanics says the atom "
    "exists in a <b>superposition of decayed and undecayed</b> states until measured &mdash; but does this mean the cat "
    "is simultaneously alive and dead?</p>"
    "<p>Schr&ouml;dinger intended this as a <b>reductio ad absurdum</b>, not a genuine claim. He found it ridiculous that "
    "quantum superposition &mdash; a property of microscopic systems &mdash; could propagate to macroscopic objects. Yet "
    "quantum mechanics offers no obvious rule for where the quantum/classical boundary lies. This is the <b>measurement "
    "problem</b>, still genuinely unresolved after a century.</p>"
    "<p>Different interpretations resolve the cat differently. In <b>Copenhagen</b>, the cat&#39;s state is undefined "
    "until observation. In <b>Many-Worlds</b>, the universe branches and the cat is alive in one branch, dead in another. "
    "<b>Objective collapse theories</b> (GRW, Penrose) introduce physical mechanisms collapsing superpositions at "
    "macroscopic scales through gravity or spontaneous localization.</p>"
    "<p><b>Decoherence theory</b> offers the most modern resolution: macroscopic objects interact with trillions of "
    "environmental particles per second, destroying quantum coherence almost instantly. The cat is effectively classical "
    "not because of mysterious collapse, but because <b>entanglement with the environment</b> makes its superposition "
    "inaccessible. The boundary is not sharp &mdash; it is a gradient governed by scale and complexity.</p>";

static const char WA12[] =
    "<p>In 1995, philosopher David Chalmers drew a distinction that cut to the heart of consciousness studies. The "
    "<b>&ldquo;easy problems&rdquo;</b> &mdash; explaining how the brain processes sensory information, integrates data, "
    "and controls behavior &mdash; are scientifically difficult but tractable: we can imagine explaining them as functions "
    "of neural machinery. The <b>&ldquo;hard problem&rdquo;</b> is different: why does any of this processing "
    "<i>feel like something</i>?</p>"
    "<p>Why does the wavelength of 700nm light not merely trigger visual cortex activity but produce the <b>subjective "
    "experience of redness</b>? Why is there an inner light at all? This irreducible quality of experience &mdash; the "
    "<i>what-it&#39;s-like-ness</i> &mdash; is what philosophers call <b>qualia</b>. We can fully describe the neural "
    "correlates of pain without ever capturing what pain actually feels like from the inside.</p>"
    "<p>The explanatory gap motivates the concept of <b>philosophical zombies</b>: beings physically identical to us but "
    "with no inner experience. If such zombies are conceivable, functionalist accounts of mind seem incomplete. Some argue "
    "this conceivability argument is flawed; others take it as evidence that consciousness cannot be reduced to physical "
    "processes by any known framework.</p>"
    "<p>The hard problem may require revolutionary concepts. Some suggest <b>panpsychism</b> &mdash; that experience is "
    "fundamental, like mass or charge. Others propose that our intuitions about inner experience are misleading. Still "
    "others think a future physics will need to incorporate <b>experience as a primitive</b>, the way relativity "
    "incorporated spacetime. After decades of debate, the hard problem remains what Chalmers called it: irreducibly hard.</p>";

static const char WA13[] =
    "<p><b>Integrated Information Theory</b> (IIT), developed by neuroscientist Giulio Tononi from 2004 onward, is the "
    "most mathematically rigorous scientific theory of consciousness yet proposed. Its central claim: consciousness is "
    "identical to <b>integrated information</b>, denoted <b>&Phi;</b> (phi). A system is conscious to the degree that "
    "its parts share more information as a whole than they do in isolation.</p>"
    "<p>IIT begins from five <b>axioms of conscious experience</b>: it exists (intrinsic existence), is structured "
    "(composition), is specific (information), is unified (integration), and is definite (exclusion). From these axioms "
    "it derives postulates any physical substrate of consciousness must satisfy. Feed-forward networks, however complex, "
    "have zero &Phi; and are therefore not conscious; tightly integrated recurrent systems generate high &Phi;.</p>"
    "<p>IIT makes striking predictions: the <b>cerebellum</b>, though containing more neurons than the cortex, "
    "contributes little to consciousness because its modular architecture yields low &Phi;. The <b>cerebral cortex&#39;s "
    "</b>rich recurrent connectivity generates the highest &Phi; in known biology. IIT also implies certain simple "
    "systems have a tiny but nonzero &Phi;, making it a form of <b>panpsychism</b>.</p>"
    "<p>Critics argue &Phi; is computationally intractable for large systems and that IIT&#39;s axioms derive from "
    "phenomenology, not physics. Nevertheless, IIT has driven rigorous programs measuring <b>cortical complexity</b> in "
    "anesthetized, sleeping, and vegetative patients, yielding genuine clinical insights into the neural basis of "
    "awareness and informing decisions in cases of disorders of consciousness.</p>";

static const char WA14[] =
    "<p><b>Panpsychism</b> is the view that mind or proto-mental properties are a fundamental and pervasive feature of "
    "reality &mdash; that consciousness, in some form, is not an emergent accident of biological evolution but an "
    "intrinsic aspect of the universe itself. Far from fringe, it has roots in Plato, Spinoza, Leibniz, and William "
    "James, and has undergone serious philosophical revival in recent decades.</p>"
    "<p>The appeal lies in sidestepping the hard problem. Rather than asking how physical processes give rise to "
    "experience from nothing, panpsychism holds that experience never arises from non-experience at all. "
    "<b>Micro-experiences</b> associated with fundamental particles combine to form the rich consciousness of biological "
    "beings. This raises the <b>combination problem</b>: how do micro-experiences combine into unified awareness?</p>"
    "<p>Philosophers like David Chalmers, Philip Goff, and Galen Strawson argue panpsychism is no more bizarre than "
    "alternatives: physics already postulates unobservable entities (quantum fields, dark matter) and adding experiential "
    "properties to fundamental particles is parsimonious. <b>Cosmopsychism</b> suggests the universe as a whole is the "
    "primary conscious entity, with individual minds as aspects of cosmic consciousness.</p>"
    "<p>Panpsychism intersects with <b>Integrated Information Theory</b>, which mathematically implies any system with "
    "nonzero &Phi; has experience. It connects to ancient traditions from <b>Vedanta</b> to animism that see mind as "
    "woven into nature. While mainstream neuroscience remains skeptical, the theory&#39;s philosophical respectability "
    "has grown substantially, appearing regularly in top philosophy journals as a serious, contested answer to the "
    "deepest mystery of mind.</p>";

static const char WA15[] =
    "<p>A <b>lucid dream</b> is a dream in which the dreamer is aware they are dreaming, often gaining voluntary control "
    "over the dream environment. Documented across cultures for millennia &mdash; Aristotle noted it around 350 BCE, "
    "Tibetan Buddhists cultivated it as <b>dream yoga</b> &mdash; scientific verification came in 1975, when Keith "
    "Hearne and later Stephen LaBerge demonstrated that lucid dreamers could signal researchers using <b>pre-agreed eye "
    "movements</b> detectable during REM sleep.</p>"
    "<p>Lucid dreams occur predominantly during <b>REM sleep</b>, when brain activity most closely resembles waking. "
    "Neuroimaging reveals heightened activity in the <b>prefrontal cortex</b> &mdash; the seat of self-reflective "
    "awareness &mdash; compared to ordinary dreaming. This prefrontal reactivation appears to be the neural correlate "
    "of self-awareness within the dream state.</p>"
    "<p>Two main induction techniques exist: <b>DILD</b> (Dream-Initiated Lucid Dream), where awareness arises "
    "spontaneously within a dream, often triggered by noticing an anomaly during a reality check; and <b>WILD</b> "
    "(Wake-Initiated Lucid Dream), where consciousness transitions directly from waking into dreaming, sometimes "
    "accompanied by <b>sleep paralysis</b> and hypnagogic hallucinations.</p>"
    "<p>Therapeutic applications are growing: lucid dreaming can reduce <b>nightmare frequency</b> in PTSD patients, "
    "who can consciously confront and alter traumatic dream content. It offers a unique philosophical window &mdash; a "
    "state where perceiver and perceived world are simultaneously recognizable as <b>products of the same mind</b>, "
    "raising profound questions about the relationship between consciousness and its own representations of reality.</p>";

static const char WA16[] =
    "<p>The <b>Default Mode Network</b> (DMN) is a system of interconnected brain regions most active when a person is "
    "<b>not engaged in goal-directed tasks</b> &mdash; during mind-wandering, daydreaming, self-reflection, and "
    "imagining the future or past. Its key nodes include the <b>medial prefrontal cortex</b>, <b>posterior cingulate "
    "cortex</b>, <b>angular gyrus</b>, and <b>hippocampal formation</b>. Marcus Raichle officially described it in 2001 "
    "after noticing these regions consistently deactivate during focused tasks.</p>"
    "<p>The DMN is the neural substrate of the <b>narrative self</b> &mdash; the chattering internal monologue, the "
    "sense of being a continuous entity across time, the rehearsal of social scenarios and personal memories. Its "
    "activity correlates with <b>rumination and self-referential thinking</b>. Its deactivation during deep "
    "concentration, flow states, or meditation corresponds to the subjective dissolution of self.</p>"
    "<p>Perhaps its most striking property is its <b>disruption by psychedelic compounds</b>. Psilocybin, LSD, and DMT "
    "all dramatically reduce DMN activity and synchrony. Users describe this as ego dissolution &mdash; the loss of the "
    "boundary between self and world. Neuroimaging shows the degree of DMN disruption correlates directly with the "
    "<b>subjective intensity of ego loss</b>.</p>"
    "<p>DMN dysfunction is implicated in <b>depression</b> (hyperactive rumination), <b>Alzheimer&#39;s disease</b> "
    "(early amyloid plaque deposition targets DMN hubs), autism spectrum disorder, and schizophrenia. The DMN may be "
    "essential for understanding both the foundations of <b>healthy self-awareness</b> and the full spectrum of its "
    "pathological extremes across the human mind.</p>";

static const char WA17[] =
    "<p><b>Psilocybin mushrooms</b> &mdash; primarily species of the genus <b>Psilocybe</b>, including <i>P. cubensis</i>, "
    "<i>P. semilanceata</i>, and over 200 others &mdash; synthesize the psychedelic compounds <b>psilocybin</b> and "
    "<b>psilocin</b>. Upon ingestion, psilocybin is dephosphorylated to psilocin, which acts as a <b>partial agonist at "
    "5-HT2A serotonin receptors</b> throughout the cortex, producing profound alterations in perception, cognition, "
    "emotion, and sense of self lasting 4&ndash;6 hours.</p>"
    "<p>Indigenous use in Mesoamerica spans thousands of years. The Mazatec people of Oaxaca use <i>teonan&aacute;catl</i> "
    "(&ldquo;sacred flesh of the gods&rdquo;) in <b>velada healing ceremonies</b>. Curandera <b>Mar&iacute;a Sabina</b> "
    "introduced the practice to Western ethnomycologist R. Gordon Wasson, whose 1957 <i>Life</i> magazine article "
    "catalyzed the psychedelic revolution. Albert Hofmann &mdash; who also synthesized LSD &mdash; first isolated and "
    "synthesized psilocybin in 1958.</p>"
    "<p>Neuroimaging reveals that psilocin dramatically reduces activity in the <b>Default Mode Network</b>. This "
    "correlates with <b>ego dissolution</b> &mdash; a loss of the boundary between self and world that users describe "
    "as mystical. The degree of DMN suppression correlates with mystical experience intensity and, remarkably, with "
    "therapeutic outcome in clinical trials.</p>"
    "<p>Modern research at Johns Hopkins, Imperial College London, and NYU has demonstrated psilocybin&#39;s efficacy "
    "for <b>treatment-resistant depression</b>, end-of-life anxiety, tobacco addiction, and alcohol use disorder. The "
    "FDA granted psilocybin <b>Breakthrough Therapy</b> designation in 2018&ndash;2019. A single guided session can "
    "produce lasting psychological change through <b>neuroplasticity enhancement</b>, increased connectome flexibility, "
    "and deep emotional processing in one transformative experience.</p>";

static const char WA18[] =
    "<p><b>Ayahuasca</b> is a psychedelic brew prepared from two Amazonian plants: the <b>Banisteriopsis caapi</b> vine, "
    "containing &beta;-carboline alkaloids (harmine, harmaline) that act as <b>monoamine oxidase inhibitors (MAOIs)</b>, "
    "and <b>Psychotria viridis</b> containing <b>N,N-dimethyltryptamine (DMT)</b>. The MAOIs prevent gut enzymes from "
    "breaking down oral DMT, making it active and producing experiences lasting 4&ndash;8 hours.</p>"
    "<p>DMT is found endogenously in the human brain and body, produced from tryptophan via specialized enzymes. When "
    "smoked or vaporized, DMT produces an overwhelming 15-minute experience of <b>geometric visuals, entity contact, and "
    "total dissolution of ordinary reality</b>. Users consistently report entering structured hyperdimensional spaces "
    "populated by intelligent beings, with a sense of significance exceeding anything in waking life.</p>"
    "<p>Indigenous Amazonian traditions &mdash; including the <b>Shipibo-Conibo, Shuar, and Santo Daime</b> churches "
    "&mdash; have used ayahuasca for centuries for healing, divination, and communication with plant spirits. The brew is "
    "considered a <b>teacher plant</b> with its own intelligence. Ceremonies are led by trained <b>curanderos</b> who "
    "use sacred songs called <i>icaros</i> to guide the experience.</p>"
    "<p>Clinical research has found ayahuasca effective for <b>treatment-resistant depression</b>, PTSD, and substance "
    "use disorders, with a single ceremony producing measurable antidepressant effects sustained for weeks. Neuroimaging "
    "during ayahuasca shows hyperactivation of <b>visual cortex</b> even with eyes closed, reduced DMN activity, and "
    "novel connectivity across brain networks. DMT&#39;s endogenous presence and extreme effects make it one of the most "
    "enigmatic molecules in neuroscience.</p>";

static const char WA19[] =
    "<p><b>Cannabis sativa</b> has been cultivated for at least 10,000 years for fiber, seed, and medicine. Its "
    "psychoactive effects arise primarily from <b>&Delta;9-tetrahydrocannabinol (THC)</b>, which binds to <b>CB1 "
    "receptors</b> throughout the brain, and <b>cannabidiol (CBD)</b>, with complex modulatory effects. The plant "
    "produces over 100 <b>phytocannabinoids</b> plus hundreds of terpenes, creating an entourage of interacting "
    "compounds that collectively shape its effects.</p>"
    "<p>The discovery of cannabinoid receptors in the late 1980s by Allyn Howlett led to identification of the "
    "<b>endocannabinoid system</b> (ECS) &mdash; a vast neuromodulatory network present throughout the brain and body, "
    "predating cannabis use by hundreds of millions of years of evolution. The primary endocannabinoids are "
    "<b>anandamide</b> (AEA, from the Sanskrit word for bliss) and <b>2-arachidonoylglycerol</b> (2-AG), synthesized "
    "on-demand from membrane lipids and acting retrogradely to regulate neurotransmitter release.</p>"
    "<p>The ECS acts as a <b>master regulator</b> of neural plasticity, modulating appetite, pain, memory, mood, immune "
    "function, and sleep. CB1 receptors are among the most densely expressed GPCRs in the brain; CB2 receptors are found "
    "primarily in immune cells, mediating anti-inflammatory effects. Cannabis essentially hijacks a system that evolved "
    "to maintain homeostasis throughout the organism.</p>"
    "<p>Clinically, <b>cannabinoids</b> are approved for nausea, spasticity (multiple sclerosis), chronic pain, and "
    "childhood epilepsy. The endocannabinoid system&#39;s role in <b>synaptic pruning</b> during adolescent brain "
    "development is why cannabis use in teens carries particular risks &mdash; disrupting a developmental process "
    "essential to healthy adult cognition and executive function.</p>";

static const char WA20[] =
    "<p><b>Peyote</b> (<i>Lophophora williamsii</i>) is a small, spineless cactus native to the Chihuahuan Desert. Its "
    "primary psychoactive alkaloid is <b>mescaline</b> (&beta;-3,4,5-trimethoxyphenethylamine), a phenethylamine acting "
    "primarily on <b>5-HT2A receptors</b>. A typical active dose is 200&ndash;400 mg, producing a 10&ndash;12 hour "
    "experience of intense visual phenomena, emotional depth, and philosophical insight unlike any other compound.</p>"
    "<p>Indigenous use of peyote spans at least 5,000 years, evidenced by archaeological finds in the Rio Grande Valley. "
    "The <b>Huichol (Wixaritari)</b> people of Nayarit and Jalisco make annual pilgrimages hundreds of miles to collect "
    "peyote in the sacred land of <b>Wirikuta</b>, incorporating it into ceremonies centered on the deer spirit. The "
    "<b>Native American Church</b>, established formally in 1918, blends peyote ritual with Christian elements and has "
    "legal protections for sacramental use in the United States.</p>"
    "<p>Mescaline was first isolated in 1897 by Arthur Heffter through self-experimentation. Aldous Huxley&#39;s 1954 "
    "book <b><i>The Doors of Perception</i></b> described his mescaline experience in philosophical terms that influenced "
    "a generation. Huxley proposed the brain acts as a <b>&ldquo;reducing valve&rdquo;</b> filtering cosmic "
    "consciousness, and that mescaline temporarily disables this filter, flooding awareness with unfiltered reality.</p>"
    "<p>The San Pedro cactus (<i>Echinopsis pachanoi</i>), used in Andean <b>curanderismo</b> for over 3,500 years, also "
    "contains mescaline. Research is limited compared to psilocybin but early data suggest mescaline has antidepressant "
    "and anxiolytic properties. The slow-growing peyote plant &mdash; which takes 15 years to reach maturity &mdash; is "
    "now <b>endangered</b> due to overharvesting and habitat loss.</p>";

static const char WA21[] =
    "<p><b>Iboga</b> (<i>Tabernanthe iboga</i>) is a rainforest shrub native to Gabon and Central Africa whose root "
    "bark contains a complex cocktail of alkaloids, most notably <b>ibogaine</b>. In the <b>Bwiti religion</b> of the "
    "Mitsogo and Fang peoples, iboga is consumed in massive doses during multi-day initiation ceremonies facilitating "
    "contact with ancestors and the deep self. The experience &mdash; lasting 24&ndash;36 hours &mdash; is described as "
    "a complete dissolution and reconstitution of the personality.</p>"
    "<p>Ibogaine acts on an extraordinary range of receptors: it antagonizes <b>NMDA glutamate receptors</b>, is a "
    "kappa-opioid agonist, blocks serotonin and sigma receptors, and crucially inhibits the <b>hERG potassium "
    "channel</b> &mdash; the cardiac liability responsible for its narrow safety margin. It is also metabolized to "
    "<b>noribogaine</b>, a long-lasting opioid receptor modulator persisting for weeks.</p>"
    "<p>The most striking property is ibogaine&#39;s ability to interrupt <b>opioid withdrawal syndrome</b> almost "
    "completely in a single session. A single ibogaine treatment can break physical dependence on heroin, fentanyl, or "
    "methadone within 24 hours &mdash; an effect no conventional medicine achieves. Preliminary data suggest efficacy "
    "for cocaine, methamphetamine, and alcohol dependence as well.</p>"
    "<p>Ibogaine is Schedule I in the United States, but clinics operate in Mexico, Canada, and Europe. The cardiac risk "
    "(roughly 1 in 300 without medical screening) has driven development of <b>18-methoxycoronaridine (18-MC)</b>, a "
    "synthetic iboga analog with anti-addictive properties but reduced cardiac liability, currently in clinical trials. "
    "The Bwiti see iboga as the <b>master plant teacher</b> &mdash; a mirror forcing every initiate to confront "
    "every truth about themselves.</p>";

static const char WA22[] =
    "<p>The <b>Mandela Effect</b> describes a phenomenon where large numbers of people share the same false memory of an "
    "event that never occurred. The term was coined by researcher <b>Fiona Broome</b> in 2009, after she discovered many "
    "people shared her false memory that <b>Nelson Mandela had died in prison in the 1980s</b>, when in fact he was "
    "released in 1990 and served as President of South Africa until 1999.</p>"
    "<p>Famous examples include the <b>Berenstain Bears</b> (widely misremembered as &ldquo;Berenstein&rdquo;), the "
    "<b>Monopoly Man</b> (widely believed to wear a monocle he never had), and the line &ldquo;Luke, I am your "
    "father&rdquo; (the actual line is &ldquo;No, I am your father&rdquo;). These errors are remarkably consistent "
    "across thousands of independent individuals who have never met.</p>"
    "<p>The scientific explanation centers on the constructive nature of <b>human memory</b>. Memory is not a recording "
    "but a reconstruction influenced by language, schema, suggestion, and the memories of others. <b>Source confusion</b> "
    "&mdash; misattributing the origin of a memory &mdash; is well-documented. Social reinforcement through shared "
    "discussion cements false memories. Elizabeth Loftus demonstrated that false memories can be implanted with high "
    "confidence through the <b>misinformation effect</b>.</p>"
    "<p>The paranormal explanation &mdash; that these cases represent bleed-through from <b>parallel universes</b> where "
    "events unfolded differently &mdash; is unfalsifiable and unnecessary. Nevertheless, it reflects genuine discomfort "
    "with memory&#39;s unreliability. If we can misremember shared facts this dramatically, how much of our "
    "<b>personal narrative</b> is confabulated? The Mandela Effect is less evidence of parallel worlds than a mirror "
    "held up to the profound fragility of human cognition.</p>";

static const char WA23[] =
    "<p><b>Synchronicity</b> is a concept introduced by psychiatrist <b>Carl Gustav Jung</b> in the 1920s and formalized "
    "in a 1952 monograph co-authored with physicist <b>Wolfgang Pauli</b>. Jung defined it as the <b>acausal connecting "
    "principle</b>: meaningful coincidences between psychic events and external happenings connected not by causality "
    "but by simultaneity and meaning. The classic example: thinking of a friend not spoken to in years, and receiving "
    "their phone call within the hour.</p>"
    "<p>Jung argued that the <b>collective unconscious</b> &mdash; a deep layer of the psyche shared across humanity, "
    "populated by archetypes &mdash; could manifest synchronistic events at pivotal moments: when a dream image appears "
    "in the external world, or when a symbol from inner work appears unexpectedly in outer life precisely at a "
    "psychologically charged moment. These events, he believed, pointed to a deeper unity between mind and matter.</p>"
    "<p>The collaboration with Pauli was significant: one of quantum mechanics&#39; founders sought a framework uniting "
    "the <b>objective world of physics</b> with the <b>subjective world of psychology</b>. They proposed a four-fold "
    "schema: causality and energy on one axis, synchronicity and meaning on another &mdash; suggesting meaning itself "
    "is a fundamental ordering principle alongside physical law.</p>"
    "<p>Modern cognitive science explains synchronicity through <b>confirmation bias</b>, <b>apophenia</b> (the tendency "
    "to perceive meaningful patterns in random data), and the <b>availability heuristic</b>: we notice coincidences that "
    "have meaning while ignoring the far more numerous occasions when the friend doesn&#39;t call. The sheer volume of "
    "daily experience guarantees occasional striking coincidences. Yet synchronicity&#39;s quality of felt significance, "
    "its timing at critical life moments, continues to fascinate and resist purely cognitive deflation.</p>";

static const char WA24[] =
    "<p>A <b>near-death experience</b> (NDE) is a vivid, structured subjective experience reported by people who have "
    "been close to death or clinically dead and resuscitated. Common features include: leaving the body and observing it "
    "from above, moving through a dark tunnel toward a <b>brilliant light</b>, encountering deceased relatives, a "
    "<b>life review</b> re-experiencing one&#39;s entire life in an instant, and a profound sense of peace, love, and "
    "cosmic significance. Reluctant return is near-universal.</p>"
    "<p>NDEs have been documented across cultures, age groups, and historical periods with strikingly similar core "
    "features. Systematic study began with Raymond Moody&#39;s 1975 <b><i>Life After Life</i></b>, which compiled "
    "accounts from cardiac arrest survivors. Kenneth Ring&#39;s Omega Project and cardiologist <b>Pim van Lommel</b>&#39;s "
    "Netherlands study followed thousands of patients with prospective rigor.</p>"
    "<p>Van Lommel&#39;s landmark 2001 <i>Lancet</i> study of 344 cardiac arrest patients found 18% reported NDE, "
    "including cases where patients accurately described their resuscitation from above the room <b>during periods of "
    "flat EEG</b>. The AWARE study attempted to verify out-of-body perception by placing hidden visual targets viewable "
    "only from above, with limited but suggestive results.</p>"
    "<p>Materialist explanations include <b>cerebral hypoxia</b>, temporal lobe activation, REM intrusion, and "
    "endogenous DMT release. None fully accounts for the consistent structure across random individuals or veridical "
    "perception claims. NDE survivors universally show reduced fear of death, increased empathy, and altered life "
    "priorities &mdash; suggesting whatever the mechanism, the experience carries genuine <b>transformative power</b> "
    "that reshapes entire lives in a matter of minutes.</p>";

static const char WA25[] =
    "<p><b>Ball lightning</b> is one of the most puzzling anomalous phenomena in atmospheric science. Witnesses describe "
    "luminous, roughly spherical objects ranging from golf-ball to beach-ball size, appearing during or after "
    "thunderstorms, moving slowly through the air, occasionally passing through windows, and lasting seconds to minutes "
    "before silently dissipating or exploding with a sharp crack. Reports span centuries and cultures.</p>"
    "<p>Despite thousands of reports, ball lightning has never been definitively captured on film or measured "
    "instrumentally under controlled conditions. A 2012 Chinese study reported accidental spectroscopic capture of what "
    "may be a natural example, showing silicon, iron, and calcium consistent with soil vaporization by lightning &mdash; "
    "though this remains contested. The elusive nature of ball lightning has led some to question whether all reports "
    "describe the same phenomenon.</p>"
    "<p>Proposed explanations are numerous and contradictory. <b>Plasma vortex</b> theories suggest self-contained "
    "plasma stabilized by magnetic fields. <b>Microwave cavity</b> models propose lightning creates microwave radiation "
    "trapped in reflective plasma shells. <b>Quantum coherence models</b> invoke macroscopic quantum states. "
    "<b>Antimatter annihilation</b> and <b>oscillating solitons</b> have been proposed and largely dismissed. Ball "
    "lightning has been reproduced in limited laboratory forms using microwave ovens and high-current discharges.</p>"
    "<p>Multiple credible witnesses &mdash; including Nobel laureate <b>Pyotr Kapitsa</b> and several military pilots "
    "&mdash; have reported direct observations. The challenge is reproducing the key features: <b>stability, mobility, "
    "and anomalous duration</b> &mdash; a plasma that persists and moves in ways no known plasma can explain. Ball "
    "lightning remains a genuine unsolved problem in physics, a reminder that the atmosphere still holds surprises "
    "for those paying careful attention.</p>";

static const char WA26[] =
    "<p>On the night of August 15, 1977, astronomer <b>Jerry Ehman</b> was reviewing printouts from the <b>Big Ear "
    "radio telescope</b> at Ohio State University when he circled a sequence of characters and wrote a single word in "
    "the margin: <b>&ldquo;Wow!&rdquo;</b> The signal was a 72-second burst of radio emission at <b>1420.456 MHz</b> "
    "&mdash; strikingly close to the 1420.405 MHz emission of neutral hydrogen, the frequency SETI researchers had long "
    "predicted extraterrestrials might use as a cosmic calling frequency.</p>"
    "<p>The signal was <b>30 times stronger than background noise</b> and displayed the characteristic rise-and-fall "
    "profile expected of a genuine point source in deep space as Earth&#39;s rotation swept the telescope beam across "
    "it. It came from the direction of the constellation Sagittarius. In every measurable way it fit the expected "
    "signature of an <b>intentional narrowband transmission from beyond the solar system</b> &mdash; the most "
    "compelling candidate SETI signal ever detected.</p>"
    "<p>Despite dozens of subsequent searches directed at the same sky region using more powerful telescopes, the signal "
    "has <b>never been detected again</b>. Big Ear itself was demolished in 1998. In 2016, astronomer Antonio Paris "
    "proposed two comets passing through the field that night could explain the signal via hydrogen clouds &mdash; but "
    "the hypothesis was disputed by multiple researchers who noted comets don&#39;t produce signals with the observed "
    "narrowband characteristics.</p>"
    "<p>The Wow! Signal remains <b>unexplained</b>. It satisfies every criterion SETI researchers established for an "
    "artificial signal. Its single detection fits models of an alien civilization transmitting a repeating signal that "
    "simply hasn&#39;t been pointed our way since. Whether it was a natural phenomenon, an instrumental artifact, or a "
    "message from a civilization that will never know if we received it &mdash; the Wow! Signal stands as the most "
    "tantalizing <b>72 seconds in the history of astronomy</b>.</p>";

static const char WA27[] =
    "<p><b>Time dilation</b> is one of the most counter-intuitive predictions of Einstein&#39;s theories of relativity, "
    "and one of the most thoroughly confirmed. Under <b>special relativity</b> (1905), time passes more slowly for any "
    "observer in motion relative to another. The faster you travel, the slower your clock ticks compared to someone at "
    "rest. At 87% of the speed of light, time passes at half the rate; at 99.9%, roughly 22 times slower. This is not "
    "an illusion or a measuring artifact &mdash; it is a real, physical difference in elapsed time.</p>"
    "<p>The <b>twin paradox</b> makes this vivid: one twin boards a spacecraft and travels at near-light speed to a "
    "distant star, then returns. The traveling twin arrives home genuinely younger than the twin who stayed on Earth. "
    "Experiments confirm this: atomic clocks flown on aircraft around the world return slightly behind ground clocks, "
    "exactly as relativity predicts. <b>GPS satellites</b> orbit at high speed and in weaker gravity, causing their "
    "clocks to run fast by about 38 microseconds per day. Without constant relativistic correction, GPS coordinates "
    "would drift by roughly 10 kilometers daily.</p>"
    "<p><b>General relativity</b> (1915) adds gravitational time dilation: clocks run slower in stronger gravitational "
    "fields. A clock on the surface of Earth ticks slightly slower than one on a mountaintop. Near a black hole, time "
    "dilation becomes extreme &mdash; an observer hovering just outside the event horizon would appear frozen to a "
    "distant watcher, experiencing infinite time dilation at the horizon itself.</p>"
    "<p>The cosmic implication is profound: <b>time is not a universal constant</b> ticking uniformly throughout the "
    "cosmos. It is a dimension woven into the fabric of spacetime, stretched by gravity and compressed by velocity. "
    "Two observers in different gravitational fields or moving at different speeds inhabit genuinely different temporal "
    "realities. The universe does not have a single master clock &mdash; only the interlocking web of local times that "
    "Einstein revealed.</p>";

static const char WA28[] =
    "<p>The <b>multiverse</b> is the hypothesis that our universe &mdash; the entire observable cosmos spanning 93 "
    "billion light-years &mdash; is just one of an enormous or infinite number of universes. It emerges not from "
    "speculation but from taking seriously the mathematics of our best-tested physical theories. <b>Eternal inflation</b> "
    "proposes that the quantum field that drove cosmic inflation never stopped everywhere at once; instead, it continues "
    "inflating in most regions while &ldquo;bubble universes&rdquo; like ours nucleate and expand within it. Each bubble "
    "is causally disconnected from the others, separated by eternally inflating space.</p>"
    "<p>String theory deepens the picture. The theory&#39;s landscape contains roughly <b>10<sup>500</sup> possible "
    "vacuum states</b>, each corresponding to a universe with different physical constants, particle masses, and "
    "dimensionality. Physicist <b>Andrei Linde&#39;s chaotic inflation</b> combines these: eternal inflation samples "
    "the string landscape, producing a vast ensemble of universes, most hostile to life, some &mdash; like ours &mdash; "
    "with finely tuned constants permitting atoms, stars, and observers.</p>"
    "<p><b>Max Tegmark</b> organizes the multiverse into four levels: Level I (regions beyond our cosmic horizon with "
    "the same laws), Level II (bubble universes with different constants), Level III (the <b>Many-Worlds</b> quantum "
    "multiverse, where every quantum event branches into parallel outcomes), and Level IV (all mathematically consistent "
    "structures exist physically). Many-Worlds, proposed by Hugh Everett in 1957, is a Level III multiverse generated "
    "by quantum mechanics itself &mdash; no new assumptions required.</p>"
    "<p>The deepest problem with the multiverse is <b>falsifiability</b>. If other universes are causally disconnected "
    "from ours, no observation can confirm or deny them &mdash; leading critics to argue it is metaphysics dressed as "
    "physics. Defenders counter that the multiverse is the unavoidable prediction of theories that are themselves "
    "testable, and that dismissing untestable implications of tested theories is philosophically inconsistent. The debate "
    "cuts to the heart of what science is and what counts as an explanation.</p>";

static const char WA29[] =
    "<p>In 2003, philosopher <b>Nick Bostrom</b> published a trilemma that has haunted physicists and philosophers "
    "ever since. His argument: at least one of three propositions must be true. Either (1) virtually all civilizations "
    "go extinct before reaching the technological maturity to run detailed simulations of minds; or (2) civilizations "
    "that reach that capability choose not to run such simulations; or (3) we are almost certainly living in a computer "
    "simulation. The logic is probabilistic &mdash; if simulations are ever run, simulated minds will vastly outnumber "
    "biological ones, making it statistically overwhelmingly likely that any given mind is simulated.</p>"
    "<p>The hypothesis has found unlikely advocates in physics. Some researchers note that the <b>Planck length</b> "
    "(~10<sup>-35</sup> meters), the smallest meaningful scale of space, behaves like a minimum pixel size for reality. "
    "<b>Quantum uncertainty</b> &mdash; the fact that particles have no definite properties until observed &mdash; "
    "resembles a rendering optimization: a simulation that only computes definite values when they are measured. The "
    "digital structure of quantum mechanics (quantized energy levels, discrete spin states) fits naturally with the "
    "simulation framework, though physicists caution these parallels may be superficial.</p>"
    "<p><b>Elon Musk</b> famously stated the probability we live in &ldquo;base reality&rdquo; is &ldquo;one in "
    "billions.&rdquo; Physicist Neil deGrasse Tyson put the odds at 50-50. Others, including George Ellis, argue the "
    "hypothesis is <b>unfalsifiable</b> and therefore not scientific in the Popperian sense &mdash; no experiment "
    "could distinguish a perfect simulation from base reality by design.</p>"
    "<p>The simulation hypothesis has a philosophical ancestor in <b>Ren&eacute; Descartes&#39;</b> evil demon thought "
    "experiment (1641): a malicious demon could be feeding us false sensory experiences, making the entire external world "
    "an illusion. The simulation hypothesis is, in a sense, a technologically updated evil demon &mdash; one that "
    "emerged from the logic of computation rather than theological speculation. Whether it is physics or philosophy "
    "depends on whom you ask &mdash; and perhaps on whether the answer is knowable at all.</p>";

static const char WA30[] =
    "<p>The <b>anthropic principle</b> addresses a profound puzzle: the universe appears fine-tuned for life. The "
    "fundamental constants of physics &mdash; the strength of gravity, the mass of the electron, the cosmological "
    "constant, the ratio of electromagnetic to gravitational force &mdash; are set to values that, if altered by even "
    "tiny fractions, would produce a cosmos of black holes, or pure hydrogen, or nothing at all. No stars, no atoms "
    "heavier than lithium, no chemistry, no life. The probability of landing on life-permitting values by chance "
    "appears astronomically small.</p>"
    "<p>The <b>weak anthropic principle</b> (Brandon Carter, 1973) offers a logical resolution: we observe the "
    "universe&#39;s constants to be life-permitting because, if they weren&#39;t, we wouldn&#39;t exist to observe "
    "them. This is not a physical explanation but a <b>selection effect</b> &mdash; the same logic by which we note "
    "we were born in an era with breathable air and liquid water. The <b>strong anthropic principle</b> goes further, "
    "claiming the universe must have properties permitting life at some point in its history.</p>"
    "<p>Perhaps the most striking example involves <b>carbon synthesis</b>. In the 1950s, astronomer Fred Hoyle "
    "predicted that carbon-12 must have an excited nuclear energy state at approximately 7.65 MeV &mdash; because "
    "without it, stellar fusion could not produce the carbon that makes life possible, yet life clearly exists. The "
    "state was found experimentally at 7.65 MeV. Hoyle said he became a theist afterward. This is the anthropic "
    "principle in action: the existence of observers constrains what physics must permit.</p>"
    "<p>The philosopher Douglas Adams parodied the fallacy of design arguments with the puddle analogy: a puddle "
    "marvels at how perfectly its hole fits it, unaware that the hole came first. But the anthropic puzzle remains "
    "serious. The <b>cosmological constant problem</b> &mdash; why vacuum energy is 120 orders of magnitude smaller "
    "than quantum field theory predicts &mdash; may require anthropic reasoning if no dynamical solution exists. The "
    "anthropic principle may be less a philosophical trick and more a constraint that any final theory of physics "
    "must satisfy.</p>";

static const char WA31[] =
    "<p>In the late 19th century, physicist <b>Ludwig Boltzmann</b> developed statistical mechanics &mdash; the theory "
    "that thermodynamic laws emerge from the statistical behavior of enormous numbers of particles. One consequence is "
    "that entropy (disorder) overwhelmingly tends to increase, defining the arrow of time. But statistical mechanics "
    "also permits, in principle, <b>spontaneous fluctuations</b> into lower-entropy states. Given enough time &mdash; "
    "truly astronomical timescales &mdash; any configuration of matter can assemble by chance.</p>"
    "<p>This leads to a deeply unsettling idea. In the far future of the universe, after stars have died and black "
    "holes have evaporated in a state approaching <b>heat death</b>, the remaining thermal fluctuations in a near-"
    "equilibrium cosmos could, over unimaginably vast timescales, spontaneously assemble a <b>Boltzmann Brain</b>: "
    "a conscious entity complete with false memories of a rich past, embedded in a body that assembled from random "
    "quantum fluctuations and will dissolve moments later. Such a brain would believe it existed in a universe just "
    "like ours &mdash; because its memories say so.</p>"
    "<p>The cosmological problem is this: a Boltzmann Brain is <b>vastly more probable</b> than an entire low-entropy "
    "universe like ours spontaneously fluctuating into existence. In a universe with infinite time and finite entropy, "
    "random brains should outnumber structured universes by factors incomprehensible to human intuition. If you are "
    "a randomly assembled conscious observer, you are far more likely to be a momentary fluctuation than an inhabitant "
    "of a four-billion-year evolutionary history &mdash; a conclusion so absurd it becomes an <b>argument against "
    "cosmological models</b> that predict near-equilibrium eternal futures.</p>"
    "<p>Modern cosmologists take Boltzmann Brains seriously as a <b>theoretical constraint</b>. Any successful "
    "cosmological model must either explain why our universe has such an exceptionally low-entropy beginning, or show "
    "why Boltzmann Brains don&#39;t dominate the census of observers. The puzzle connects the arrow of time, the "
    "origin of the universe, and the nature of consciousness in one vertiginous knot that has not been untied.</p>";

static const char WA32[] =
    "<p>In 1900, sponge divers working near the small Greek island of Antikythera discovered the wreck of a Roman-era "
    "cargo ship. Among the recovered artifacts was a corroded lump of bronze that sat in an Athens museum for decades "
    "before researchers realized they were looking at the most sophisticated mechanism to survive from antiquity. The "
    "<b>Antikythera Mechanism</b>, now dated to roughly 100 BCE, is a hand-cranked bronze computer with at least "
    "<b>37 interlocking gears</b> of extraordinary precision.</p>"
    "<p>Modern analysis using X-ray tomography and polynomial texture mapping revealed its function in extraordinary "
    "detail. The device computed the <b>solar and lunar calendars</b>, tracked the 223-month <b>Saros cycle</b> to "
    "predict solar and lunar eclipses, displayed the position of the Moon against the zodiac, tracked the four-year "
    "cycle of the <b>Panhellenic games</b> (including the Olympics), and &mdash; most remarkably &mdash; modeled the "
    "positions of at least five planets using epicyclic gearing. A pin-and-slot mechanism reproduced the Moon&#39;s "
    "elliptical orbit, encoding Hipparchus&#39;s lunar theory in bronze.</p>"
    "<p>The mechanism&#39;s technical sophistication has no parallel in the ancient world. The <b>next comparable "
    "gear mechanism</b> does not appear in the historical record for approximately 1,400 years &mdash; in medieval "
    "European astronomical clocks. This gap implies either that the Antikythera Mechanism was an isolated genius of "
    "engineering whose tradition was lost, or that the ancient Greek technological tradition was far more advanced "
    "than the surviving record suggests.</p>"
    "<p>Eighty-two fragments survive, representing perhaps a third of the original device. Ongoing research by the "
    "Antikythera Research Team continues to decode inscription text on its surfaces &mdash; a user manual in ancient "
    "Greek. The mechanism stands as a humbling reminder that human ingenuity is ancient, and that the history of "
    "technology is riddled with <b>lost peaks</b> that archaeology has only partially recovered.</p>";

static const char WA33[] =
    "<p><b>Nikola Tesla</b> (1856&ndash;1943) was a Serbian-American inventor and electrical engineer whose "
    "contributions reshaped the modern world, though he died penniless and largely forgotten in a New York hotel room. "
    "His greatest achievement &mdash; the <b>alternating current (AC) electrical system</b> &mdash; won the War of "
    "Currents against Thomas Edison&#39;s direct current (DC) infrastructure and became the global standard for "
    "electrical power transmission. Tesla&#39;s polyphase AC motor and transformer system, commercialized by "
    "Westinghouse, powered the 1893 World&#39;s Fair in Chicago and subsequently the entire industrialized world.</p>"
    "<p>Tesla&#39;s laboratory inventions included the <b>Tesla coil</b> (a resonant transformer producing high-"
    "voltage, high-frequency alternating current), the radio (he holds the original patent, awarded to him by the "
    "US Supreme Court in 1943, the year he died), early fluorescent lighting, and remote control. His most ambitious "
    "project was the <b>Wardenclyffe Tower</b> (1901&ndash;1917) on Long Island &mdash; a 57-meter transmission tower "
    "intended to broadcast wireless communications and, Tesla claimed, unlimited free electrical power to the entire "
    "globe through the Earth&#39;s ionosphere. His backer J.P. Morgan pulled funding when it became clear the system "
    "could not be metered for profit.</p>"
    "<p>Tesla claimed to have built a <b>mechanical resonance oscillator</b> that could shake buildings by finding "
    "their resonant frequency &mdash; the &ldquo;earthquake machine&rdquo; &mdash; and designed a <b>charged particle "
    "beam weapon</b> (the &ldquo;death ray&rdquo;) in his final years. Whether these were functional inventions or "
    "promotional fantasies is debated. His mental health deteriorated in later life; he became obsessed with pigeons "
    "and exhibited signs of OCD.</p>"
    "<p>When Tesla died in 1943, the <b>FBI seized his papers</b> under the Alien Property Custodian Act, citing "
    "national security. Most were eventually returned to the Nikola Tesla Museum in Belgrade. His modern rehabilitation "
    "is total: the SI unit of magnetic flux density is the tesla, electric vehicle company Tesla Motors bears his "
    "name, and wireless charging &mdash; the technology he envisioned a century before its realization &mdash; is "
    "now in every pocket in the world.</p>";

static const char WA34[] =
    "<p>In July 1518, a woman named <b>Frau Troffea</b> stepped into a street in Strasbourg (then part of the Holy "
    "Roman Empire) and began to dance. She danced through the day and into the night, unable or unwilling to stop. "
    "Within a week, 34 people had joined her. Within a month, approximately <b>400 people</b> were dancing "
    "uncontrollably through the streets of Strasbourg, many collapsing from exhaustion only to resume when revived. "
    "Contemporary accounts record that some dancers died &mdash; of heart attacks, strokes, and sheer physical "
    "collapse &mdash; after days of continuous movement.</p>"
    "<p>The authorities&#39; response made the situation worse: physicians declared the affliction a &ldquo;hot "
    "blood&rdquo; disease requiring the afflicted to dance it out, so the city <b>hired musicians, erected a stage, "
    "and brought in professional dancers</b> to keep the victims moving. Guild halls were cleared for dancing. The "
    "epidemic persisted for weeks before finally dying out, as mysteriously as it began.</p>"
    "<p>Modern historians and medical researchers have proposed several explanations. <b>Mass psychogenic illness</b> "
    "(historically called mass hysteria) is the leading theory: under extreme psychological stress &mdash; and 1518 "
    "Strasbourg was suffering famine, plague, and smallpox simultaneously &mdash; communities can exhibit collective "
    "involuntary behaviors. A second theory invokes <b>ergot fungus</b> (Claviceps purpurea), which infects rye bread "
    "and contains compounds related to LSD, producing convulsions and spasms. The same fungus has been linked to the "
    "Salem witch trial accusations of 1692.</p>"
    "<p>Many of the dancers reportedly believed they were cursed by <b>St. Vitus</b>, a Christian martyr associated "
    "with involuntary movement (the neurological condition Sydenham&#39;s chorea was long called St. Vitus&#39; Dance). "
    "The 1518 outbreak was not unique &mdash; similar dancing plagues are recorded across the Rhine Valley in the "
    "14th and 15th centuries. The dancing plague remains one of history&#39;s most bizarre and well-documented "
    "episodes of <b>collective human behavior</b>.</p>";

static const char WA35[] =
    "<p><b>John Whiteside Parsons</b> (1914&ndash;1952) lived one of the most extraordinary double lives in the "
    "history of science. As a self-taught chemist and rocket engineer, he co-founded what became the <b>Jet Propulsion "
    "Laboratory (JPL)</b> in Pasadena, California, and pioneered the development of solid rocket fuels. His invention "
    "of the first <b>castable composite rocket propellant</b> &mdash; a stable, moldable solid fuel that could be "
    "formed into any shape &mdash; was a fundamental breakthrough that made modern rocketry possible. JATO (jet-"
    "assisted takeoff) units he developed gave heavily loaded aircraft the thrust to become airborne, and his "
    "formulations remain the conceptual basis for solid rocket boosters used today.</p>"
    "<p>Parsons was simultaneously one of the most devoted American followers of the occultist <b>Aleister Crowley</b> "
    "and a senior member of the <b>Ordo Templi Orientis</b> (OTO). He performed elaborate Thelemic rituals in his "
    "Pasadena mansion, which also functioned as a boarding house that attracted artists, anarchists, and occultists. "
    "In 1946, Parsons performed the <b>Babalon Working</b> &mdash; a series of magical rituals intended to incarnate "
    "a divine feminine force &mdash; with a collaborator named <b>L. Ron Hubbard</b>, who would go on to found "
    "Scientology. The FBI had been monitoring Parsons for years.</p>"
    "<p>Crowley himself was alarmed by Parsons&#39; behavior, writing that he feared Parsons had been &ldquo;led "
    "astray.&rdquo; Parsons&#39; security clearance was revoked in 1952 after a loyalty investigation. He was found "
    "dead that same year in his home laboratory following an explosion &mdash; officially ruled an accident involving "
    "fulminate of mercury, but <b>widely speculated</b> to have been otherwise. He was 37.</p>"
    "<p>His legacy endures in the most official ways. The <b>JPL cafeteria is named after him</b>. The crater Parsons "
    "on the far side of the Moon bears his name. His biography &mdash; rocket scientist by day, magician by night "
    "&mdash; is not an anomaly of the fringe but a documented feature of American technological history, preserved in "
    "the archives of both NASA&#39;s founding institution and the FBI.</p>";

static const char WA36[] =
    "<p>The <b>Voynich Manuscript</b> is a 240-page illustrated codex held at Yale University&#39;s Beinecke Rare "
    "Book &amp; Manuscript Library. Carbon dating places the vellum at 1404&ndash;1438 CE. It is written in a "
    "continuous, apparently fluent script using an <b>alphabet found nowhere else in the world</b>, in a language "
    "that has resisted decipherment by every cryptographer, linguist, and computer scientist who has attempted it "
    "for over a century.</p>"
    "<p>Its illustrations divide it into sections: a <b>herbal section</b> with drawings of plants that match no "
    "known species; an <b>astronomical section</b> with circular diagrams resembling star charts and zodiac wheels; "
    "a <b>biological section</b> depicting naked female figures bathing in pools connected by elaborate tube systems; "
    "a <b>cosmological section</b> with fold-out illustrations; and a <b>pharmaceutical section</b> with drawings of "
    "roots and containers. The illustrations suggest a real purpose &mdash; someone went to extraordinary effort to "
    "produce this book &mdash; yet their meaning remains opaque.</p>"
    "<p>The manuscript&#39;s history is fragmentary. It was purchased in 1912 by antiquarian <b>Wilfrid Voynich</b> "
    "from a Jesuit college in Italy. A letter found with it suggests it was once owned by Holy Roman Emperor "
    "Rudolf II, who reportedly paid 600 gold ducats for it. It was analyzed by WWII cryptographers at the US Army "
    "Signal Corps, by NSA linguists, and more recently by machine learning models. <b>All have failed</b> to extract "
    "meaning.</p>"
    "<p>Statistical analysis reveals the text is not random: it follows <b>Zipf&#39;s law</b> (a property of natural "
    "languages where word frequency inversely correlates with rank), has consistent word length distributions, and "
    "shows structural patterns suggesting grammar. Theories range from an elaborate hoax to an encoded herbal "
    "compendium, a glossolalia text, a constructed language, or an unknown natural language. In 2019, a University "
    "of Bristol researcher proposed the text was proto-Romance language encoded with anagram substitution &mdash; but "
    "the claim was not widely accepted. The Voynich Manuscript remains one of the most studied and least understood "
    "documents in human history.</p>";

static const char WA37[] =
    "<p>On July 4, 2012, physicists at CERN&#39;s Large Hadron Collider announced the detection of a new particle "
    "consistent with the <b>Higgs boson</b> &mdash; the culmination of a 48-year theoretical hunt and one of the "
    "greatest experimental achievements in science. The Higgs was independently proposed in 1964 by <b>Peter Higgs</b>, "
    "Fran&ccedil;ois Englert, Robert Brout, and three American physicists (Hagen, Guralnik, Kibble). Higgs and Englert "
    "shared the Nobel Prize in Physics in 2013. The particle was detected at a mass of approximately <b>125 GeV</b>.</p>"
    "<p>The Higgs boson is the quantum excitation of the <b>Higgs field</b>, an invisible field that permeates all of "
    "space. Particles acquire mass through their interaction with this field &mdash; the stronger the interaction, the "
    "more massive the particle. Photons don&#39;t interact with the Higgs field at all, which is why they are massless "
    "and travel at the speed of light. Without the Higgs mechanism, all fundamental particles would be massless, travel "
    "at light speed, and be incapable of forming atoms, molecules, stars, or any structure we recognize as matter.</p>"
    "<p>The nickname &ldquo;<b>God particle</b>&rdquo; came from physicist Leon Lederman&#39;s 1993 book, originally "
    "titled &ldquo;the Goddamn Particle&rdquo; because it was so elusive &mdash; his publisher shortened it. Most "
    "physicists dislike the nickname for implying religious significance where there is none; the Higgs field is not "
    "special in kind, only in consequence.</p>"
    "<p>The discovery completed the <b>Standard Model</b> of particle physics &mdash; the theoretical framework "
    "describing all known fundamental particles and forces (except gravity). Yet the Standard Model itself is known "
    "to be incomplete: it says nothing about dark matter, dark energy, gravity at quantum scales, or why matter "
    "dominates antimatter. The Higgs boson is both the capstone of one era and a doorway to the next, unresolved "
    "layer of physics.</p>";

static const char WA38[] =
    "<p><b>Acoustic levitation</b> uses sound waves to suspend objects in mid-air against gravity &mdash; no magnets, "
    "no air jets, no physical contact. The principle rests on <b>standing waves</b> created between an ultrasonic "
    "transducer and a reflector (or between opposing transducers). Where two waves interfere to create a node of zero "
    "net pressure displacement, small objects experience a net acoustic radiation pressure that can counteract "
    "gravitational force. The effect works because the acoustic radiation pressure at a pressure node can exceed "
    "the gravitational force on sufficiently light, small objects.</p>"
    "<p>Researchers have levitated <b>water droplets, polystyrene beads, small crystals, live ants, and small "
    "fish</b> using acoustic traps. <b>NASA</b> has used acoustic levitation to study the containerless behavior of "
    "fluids in simulated microgravity &mdash; eliminating container-wall interactions that complicate results. In "
    "<b>pharmaceutical manufacturing</b>, acoustic levitation allows drug compounds to be studied and mixed without "
    "ever touching a surface, eliminating contamination and the nucleation effects that alter crystallization.</p>"
    "<p>Modern <b>phased array</b> acoustic levitators use dozens of independently controlled ultrasonic transducers "
    "whose phases can be dynamically adjusted, allowing levitated objects to be moved, rotated, and manipulated in "
    "three-dimensional space &mdash; a kind of acoustic tractor beam. Researchers at the University of Bristol "
    "demonstrated levitation and translation of objects up to 4mm in diameter in open air using a handheld "
    "phased array device.</p>"
    "<p>Potential applications extend to <b>semiconductor fabrication</b> (handling delicate components without "
    "mechanical contact), drug delivery (acoustically guided particles in biological tissue), and microfluidics. "
    "The technology scales poorly with object size &mdash; levitating a coffee cup would require impractical power "
    "levels &mdash; but for the manipulation of small objects in controlled environments, acoustic levitation "
    "offers a genuinely contactless alternative to mechanical handling, making the invisible force of sound into "
    "a practical tool.</p>";

static const char WA39[] =
    "<p>The <b>European Organization for Nuclear Research</b> &mdash; known by its French acronym <b>CERN</b> &mdash; "
    "was founded in 1954 near Geneva, Switzerland, and has grown into the world&#39;s largest particle physics "
    "laboratory. Its flagship instrument, the <b>Large Hadron Collider (LHC)</b>, is a circular accelerator 27 "
    "kilometers in circumference buried 100 meters underground, straddling the French-Swiss border. The LHC "
    "accelerates protons to <b>99.9999991% of the speed of light</b> and collides them at energies up to 13.6 "
    "teraelectronvolts &mdash; recreating conditions from fractions of a second after the Big Bang.</p>"
    "<p>The LHC&#39;s most celebrated discovery was the <b>Higgs boson</b> (2012), confirming the last missing piece "
    "of the Standard Model. CERN also produces and traps <b>antimatter</b>: the ALPHA experiment held antihydrogen "
    "atoms for 17 minutes in 2011, studying whether antimatter falls upward or downward under gravity (it falls "
    "downward, as expected, confirmed 2023). The ALICE detector studies <b>quark-gluon plasma</b> &mdash; the "
    "state of matter that existed microseconds after the Big Bang, when quarks and gluons had not yet condensed "
    "into protons and neutrons.</p>"
    "<p>CERN has an underappreciated legacy: the <b>World Wide Web</b> was invented there. In 1989, British "
    "physicist <b>Tim Berners-Lee</b> proposed a hypertext information management system to help CERN&#39;s "
    "scientists share data across different computers. By 1991, the first web pages were live. The internet&#39;s "
    "underlying protocols predate CERN, but the Web &mdash; the graphical, hyperlinked layer that most people mean "
    "when they say &ldquo;the internet&rdquo; &mdash; was born in a physics laboratory as a tool for particle "
    "physicists.</p>"
    "<p>Conspiracy theories that the LHC could create dangerous <b>black holes or open dimensional portals</b> were "
    "evaluated by a CERN safety panel and independently by physicists worldwide. At LHC energies, any microscopic "
    "black holes that could theoretically form would evaporate via Hawking radiation in less than 10<sup>-23</sup> "
    "seconds and carry no more energy than a flying mosquito. More energetic cosmic ray collisions have been "
    "occurring in Earth&#39;s atmosphere for billions of years without catastrophic effect.</p>";

static const char WA40[] =
    "<p>In 1787, physicist <b>Ernst Chladni</b> published a discovery that would mesmerize scientists for two "
    "centuries: when a metal plate dusted with sand is set vibrating with a violin bow, the sand migrates from "
    "vibrating regions to the still points &mdash; the <b>nodes</b> of the standing wave &mdash; forming elegant "
    "geometric patterns. Simple tones produce simple symmetric shapes; complex harmonics produce intricate mandalas. "
    "These <b>Chladni figures</b> are the visible signature of wave physics made tangible in sand.</p>"
    "<p>Swiss physician <b>Hans Jenny</b> (1904&ndash;1972) extended this work into a discipline he named "
    "<b>cymatics</b> (from the Greek kyma, wave). Using a tonoscope &mdash; a device that vibrates a membrane &mdash; "
    "and various liquid and powder media, Jenny documented how different frequencies consistently produce different "
    "geometric forms: circles, hexagons, mandalas, branching dendrites. Sine waves produce simple shapes; "
    "harmonically complex tones produce correspondingly complex geometries. His photographs, published in "
    "<i>Cymatics</i> (1967), became iconic in both science communication and New Age culture.</p>"
    "<p>The physics is well understood: sand and powder accumulate at nodal lines where the standing wave has "
    "zero displacement, because vibrating regions throw particles away while nodes are the only stable positions. "
    "The patterns are <b>entirely predictable</b> from wave mechanics and the geometry of the vibrating surface. "
    "Nevertheless, cymatics has been interpreted by some as evidence that sound literally creates form and that "
    "vibrational frequencies underlie all physical structure &mdash; a modern echo of <b>Pythagorean</b> music "
    "of the spheres.</p>"
    "<p>Scientific applications are real and practical. Cymatics-inspired thinking contributes to <b>acoustic "
    "engineering</b> (designing resonance-resistant structures), <b>materials science</b> (self-organization of "
    "particles), and <b>medical imaging</b>. The cymatic patterns that appear in ultrasonic standing-wave levitation "
    "experiments are the same Chladni figures rendered three-dimensionally. Whether or not vibration is the "
    "fundamental nature of reality, it is undeniably a pervasive organizing principle in physical systems.</p>";

static const char WA41[] =
    "<p>Classical computers store information as <b>bits</b> &mdash; each either 0 or 1. <b>Quantum computers</b> "
    "use <b>qubits</b>, which exploit quantum superposition to exist in a combination of 0 and 1 simultaneously "
    "until measured. A system of n qubits can represent 2<sup>n</sup> states at once, enabling certain computations "
    "to scale exponentially. Two additional quantum properties drive the advantage: <b>entanglement</b> links qubits "
    "so that operations on one instantaneously affect correlated others, and <b>quantum interference</b> allows "
    "algorithms to amplify probability amplitudes of correct answers while canceling incorrect ones.</p>"
    "<p>Two algorithms illustrate the potential. <b>Shor&#39;s algorithm</b> (1994) could factor large numbers "
    "exponentially faster than any classical algorithm &mdash; directly threatening RSA encryption, which relies on "
    "the computational difficulty of factoring. <b>Grover&#39;s algorithm</b> searches unsorted databases in "
    "square-root time. Quantum simulation &mdash; using quantum systems to model other quantum systems &mdash; "
    "promises to transform drug discovery, materials design, and chemical modeling.</p>"
    "<p>The practical challenge is <b>decoherence</b>: qubits are extraordinarily fragile, losing their quantum "
    "state through any interaction with the environment. Current (2024) quantum computers have hundreds to thousands "
    "of <b>physical qubits</b>, but noise requires massive error correction: roughly 1,000 error-prone physical "
    "qubits are needed per reliable <b>logical qubit</b>. Google&#39;s <b>Willow chip</b> (2024) demonstrated a "
    "key milestone in error correction, showing that adding more qubits reduces error rates rather than "
    "compounding them &mdash; a prerequisite for practical quantum computing.</p>"
    "<p>The timeline to fault-tolerant quantum computers capable of breaking encryption remains uncertain &mdash; "
    "estimates range from 10 to 30 years. In the interim, <b>hybrid classical-quantum algorithms</b> are finding "
    "near-term applications in optimization, finance, and molecular simulation. Governments worldwide have launched "
    "multi-billion-dollar quantum programs, not least because the nation that achieves large-scale quantum computing "
    "first gains the ability to decrypt much of the world&#39;s current encrypted communications.</p>";

static const char WA42[] =
    "<p><b>Sacred geometry</b> is the study of geometric forms, ratios, and patterns believed to underlie the "
    "fundamental structure of reality. Its central figure is the <b>golden ratio</b> (phi, approximately 1.618), "
    "the irrational number defined by the proportion a/b = (a+b)/a. It appears in the spiral of nautilus shells, "
    "the arrangement of sunflower seeds and pine cone scales (<b>phyllotaxis</b>), the branching of trees, the "
    "spiral arms of galaxies, and &mdash; with varying precision &mdash; in the proportions of the human body, the "
    "Parthenon, and the Great Pyramid. The <b>Fibonacci sequence</b> (1, 1, 2, 3, 5, 8, 13...) converges to phi "
    "as it extends &mdash; connecting integer arithmetic to geometric proportion.</p>"
    "<p><b>Plato</b> linked the five <b>Platonic solids</b> (tetrahedron, cube, octahedron, dodecahedron, "
    "icosahedron) to the classical elements (fire, earth, air, water, aether). These are the only perfectly regular "
    "three-dimensional polyhedra, their existence a theorem of pure mathematics that Plato interpreted as evidence "
    "of a geometric intelligence underlying creation. Kepler later attempted to model planetary orbits using "
    "nested Platonic solids &mdash; an idea that failed, but in failing led him toward the correct elliptical orbits.</p>"
    "<p>The <b>Flower of Life</b> &mdash; a pattern of overlapping circles arranged in hexagonal symmetry &mdash; "
    "appears carved in the Temple of Osiris at Abydos (Egypt), in the Forbidden City (China), and in medieval "
    "European cathedrals. The pattern arises naturally from the mathematics of close-packing circles, which is "
    "why it appears across cultures: it is not transmitted mystical knowledge but <b>emergent geometry</b> that "
    "any culture exploring circle construction will encounter.</p>"
    "<p>Modern mathematics finds golden-ratio proportions in contexts from <b>quasicrystals</b> (whose diffraction "
    "patterns show five-fold symmetry, impossible in classical crystals but naturally described by Penrose tilings "
    "built on phi) to financial market analysis. Whether these proportions are cosmically special or merely "
    "the signature of growth optimization and packing efficiency &mdash; phi is the most &ldquo;irrational&rdquo; "
    "number, making it optimal for avoiding resonance in biological structures &mdash; sacred geometry may be "
    "ordinary geometry recognizing the deep structure of <b>physical law</b>.</p>";

static const char WA43[] =
    "<p><b>Alchemy</b> is the ancient proto-scientific tradition that sought to transform matter &mdash; most "
    "famously to transmute base metals such as lead into gold &mdash; and to discover the <b>Philosopher&#39;s "
    "Stone</b>, a substance that could perfect any metal and confer immortality through the <b>Elixir of Life</b>. "
    "Practiced across the Islamic world, China, India, and Europe from approximately 300 CE through the 17th century, "
    "alchemy blended chemistry, philosophy, mysticism, and medicine into a single investigative tradition.</p>"
    "<p>The literal goals were never achieved. But in pursuing them, alchemists built the foundations of experimental "
    "chemistry. They invented <b>laboratory apparatus</b> still in use: the alembic (for distillation), the bain-"
    "marie (water bath for gentle heating), crucibles, and retorts. They isolated <b>mineral acids</b> &mdash; "
    "sulfuric, nitric, and hydrochloric &mdash; whose discovery transformed metallurgy and industry. They identified "
    "and characterized the elements phosphorus, antimony, and bismuth. They developed crystallization, sublimation, "
    "and filtration techniques that became standard laboratory practice.</p>"
    "<p><b>Isaac Newton</b> is perhaps the most startling example of alchemical devotion. He wrote over a million "
    "words of alchemical notes, more than he wrote on physics or mathematics. He conducted alchemical experiments "
    "for decades and kept them largely secret. Some historians argue his thinking about <b>action at a distance</b> "
    "(gravity operating through empty space, which his contemporaries found absurd) was shaped by alchemical "
    "concepts of occult forces operating between substances.</p>"
    "<p>Carl Jung interpreted alchemy as an unconscious <b>projection of psychological processes</b>. The "
    "alchemist&#39;s work of transforming base matter into gold, Jung argued, was a symbolic enactment of the "
    "<b>individuation process</b> &mdash; the transformation of the crude, unreflective self into an integrated "
    "personality. His <i>Psychology and Alchemy</i> (1944) remains influential in both Jungian psychology and "
    "the history of science. Alchemy stands as history&#39;s most productive failed project &mdash; a tradition "
    "whose pursuit of the impossible generated the real.</p>";

static const char WA44[] =
    "<p>The <b>Hermetic tradition</b> derives from texts attributed to <b>Hermes Trismegistus</b> &mdash; "
    "&ldquo;Thrice-Greatest Hermes&rdquo; &mdash; a legendary figure blending the Greek god Hermes with the "
    "Egyptian god Thoth, both patrons of wisdom, writing, and magic. The core texts, the <b>Corpus Hermeticum</b>, "
    "were written in Greek in Egypt during the 2nd and 3rd centuries CE, rediscovered in Byzantine manuscripts, "
    "and translated into Latin in 1463 by Marsilio Ficino at the request of Cosimo de&#39; Medici, igniting "
    "Renaissance Neoplatonism.</p>"
    "<p>The Hermetic tradition articulates <b>seven principles</b>, systematized in the 1908 text <i>The Kybalion</i>: "
    "<b>Mentalism</b> (the universe is mental in nature &mdash; &ldquo;All is Mind&rdquo;); <b>Correspondence</b> "
    "(&ldquo;As above, so below; as below, so above&rdquo;); <b>Vibration</b> (nothing is at rest, everything "
    "vibrates); <b>Polarity</b> (opposites are identical in nature, different in degree &mdash; heat and cold are "
    "the same thing at different poles); <b>Rhythm</b> (everything has tides and cycles); <b>Cause and Effect</b> "
    "(nothing happens by chance); and <b>Gender</b> (masculine and feminine principles operate in everything).</p>"
    "<p>The principle of <b>Correspondence</b> &mdash; &ldquo;as above, so below&rdquo; &mdash; resonates "
    "unexpectedly with modern physics. The same mathematical laws govern the subatomic and the cosmic: the "
    "equations describing electron orbits around nuclei are formally identical to those describing planetary "
    "orbits around stars, scaled by the relevant forces. Fractal geometry reveals self-similar structures across "
    "scales from neurons to galaxy filaments. Whether this is evidence of deep correspondence or merely the "
    "universality of differential equations is a matter of interpretation.</p>"
    "<p>Hermeticism influenced <b>Rosicrucianism</b>, <b>Freemasonry</b>, and much of the Western esoteric "
    "tradition. Renaissance scientists &mdash; including Copernicus, Kepler, Bruno, and Newton &mdash; were "
    "steeped in Hermetic thought. Giordano Bruno was burned at the stake in 1600 partly for Hermetic views "
    "on an infinite universe filled with worlds. The line between Hermetic philosophy and the scientific "
    "revolution is not a line at all but a <b>gradual emergence</b> of one from the other.</p>";

static const char WA45[] =
    "<p><b>Pythagoras of Samos</b> (c. 570&ndash;495 BCE) founded a secretive philosophical brotherhood "
    "that held an astonishing conviction: numbers are not merely tools for counting but the <b>fundamental "
    "substance of reality</b>. &ldquo;All is number,&rdquo; the Pythagoreans declared. They held certain "
    "numbers sacred: the tetractys (the triangular arrangement 1+2+3+4=10) was an object of worship. They "
    "discovered that musical consonance arises from simple integer ratios of string lengths &mdash; the "
    "octave (2:1), the fifth (3:2), the fourth (4:3) &mdash; and concluded that the cosmos is organized "
    "by the same harmonic mathematics: the <b>Music of the Spheres</b>.</p>"
    "<p>In 1960, physicist <b>Eugene Wigner</b> published his famous essay on the <b>&ldquo;unreasonable "
    "effectiveness of mathematics&rdquo;</b> in describing physical reality. Why should abstract structures "
    "invented by pure mathematicians &mdash; complex numbers, non-Euclidean geometry, Lie groups &mdash; turn "
    "out to be precisely the tools needed to describe quantum mechanics, general relativity, and particle "
    "physics? Wigner called this a &ldquo;wonderful gift which we neither understand nor deserve.&rdquo; "
    "The Pythagoreans would have recognized the mystery immediately.</p>"
    "<p>The <b>fine structure constant</b> (alpha ≈ 1/137.036) is a dimensionless number characterizing the "
    "strength of the electromagnetic force. It is pure number &mdash; its value is the same regardless of "
    "what units you use &mdash; and yet it precisely governs how electrons interact with light, determining "
    "atomic structure and chemistry. Richard Feynman described it as &ldquo;<b>one of the greatest damn "
    "mysteries in physics</b>&rdquo; and wrote that physicists put it on their wall and worry about it. "
    "No theory explains why it has the value it does.</p>"
    "<p><b>Gematria</b>, the Kabbalistic practice of assigning numerical values to Hebrew letters and finding "
    "meaning in the resulting sums, applies number mysticism to sacred texts. The most radical modern heir of "
    "Pythagorean thought is physicist <b>Max Tegmark&#39;s Mathematical Universe Hypothesis</b>: the physical "
    "universe is not merely described by mathematics but literally <b>is</b> a mathematical structure. There "
    "is no &ldquo;stuff&rdquo; that the equations describe &mdash; the equations are all there is. Pythagoras, "
    "27 centuries later, remains genuinely competitive.</p>";

static const char WA46[] =
    "<p>For most of human history, <b>astrology and astronomy were a single discipline</b>. The Babylonian "
    "astronomers of 700 BCE who invented the 12-sign zodiac, catalogued stars with remarkable precision, "
    "tracked the synodic periods of the planets, and developed the Saros cycle for eclipse prediction did so "
    "entirely in service of astrological interpretation. The sky was a text; the astronomer&#39;s job was "
    "to read it. Celestial mechanics and celestial divination were inseparable.</p>"
    "<p><b>Claudius Ptolemy</b> (c. 100&ndash;170 CE) wrote the <i>Almagest</i>, the foundational text of "
    "Western astronomy used for 1,400 years, and also the <i>Tetrabiblos</i>, the foundational text of "
    "Western astrology. <b>Johannes Kepler</b>, who discovered the three laws of planetary motion, cast "
    "horoscopes professionally throughout his career &mdash; for Emperor Rudolf II, among others &mdash; and "
    "regarded astrology as a potentially valid science in need of reform. <b>Galileo</b> taught medical "
    "astrology at the University of Padua as part of the standard curriculum. The separation of astronomy "
    "from astrology only fully consolidated in the 17th and 18th centuries.</p>"
    "<p>The transition occurred not because astrology was disproved but because the <b>mechanistic worldview</b> "
    "displaced the symbolic one. Kepler&#39;s elliptical orbits and Newton&#39;s universal gravitation "
    "described planets as physical bodies obeying mathematical laws, not as symbolic agents influencing "
    "human affairs. The universe became a machine &mdash; beautiful, lawful, but indifferent to individual "
    "human lives.</p>"
    "<p>Modern <b>statistical studies</b> &mdash; including large-scale analyses of thousands of birth charts "
    "against personality inventories, professional achievements, and life outcomes &mdash; have consistently "
    "found <b>no correlation</b> beyond chance. Yet astrology persists as one of humanity&#39;s oldest and "
    "most universal symbolic systems, practiced on every inhabited continent for over two millennia. It "
    "endures not as a predictive science but as a <b>symbolic language</b> for self-reflection &mdash; a "
    "mythology of the self mapped onto the geometry of the cosmos.</p>";

static const char WA47[] =
    "<p><b>Tardigrades</b> (phylum Tardigrada) are microscopic eight-legged animals, typically 0.1 to 1.5 mm "
    "in length, that have earned the distinction of being the most <b>indestructible animals known to science</b>. "
    "First described in 1773 by Johann August Ephraim Goeze (&ldquo;little water bears&rdquo;), they have been "
    "found in every habitat on Earth: Antarctic ice sheets, deep-ocean trenches at 6,000 meters, hot springs "
    "at 151&deg;C, the Himalayas at 6,000 meters elevation, tropical rainforests, and the human gut.</p>"
    "<p>Their secret is <b>cryptobiosis</b> &mdash; a reversible suspension of all metabolic processes. When "
    "conditions become lethal, tardigrades expel nearly all of their body water (reducing it to less than 1%), "
    "retract their legs, and curl into a desiccated barrel shape called a <b>tun</b>. In this state, metabolism "
    "drops to 0.01% of normal. They can remain viable in the tun state for decades &mdash; dried specimens "
    "revived after 30 years in museum collections have produced viable offspring.</p>"
    "<p>In tun form, tardigrades have survived temperatures from <b>-272&deg;C</b> (within one degree of "
    "absolute zero) to <b>+150&deg;C</b>, radiation doses of 570,000 rads (1,000 rads is lethal to humans), "
    "pressures six times greater than the Mariana Trench, and the <b>vacuum of outer space</b>. In 2007, the "
    "European Space Agency&#39;s FOTON-M3 mission exposed tardigrades to open space for 10 days; the majority "
    "survived, and some subsequently reproduced successfully. Tardigrades carry genes acquired from bacteria "
    "and fungi through horizontal gene transfer that help repair DNA damage.</p>"
    "<p>The evolutionary record suggests tardigrades have survived all <b>five mass extinction events</b> in "
    "Earth&#39;s history. Astrobiologists note that if tardigrades can survive the conditions of space, the "
    "criteria for planetary habitability may need revision. Any planet with liquid water and organic chemistry, "
    "however extreme by human standards, may be a candidate for tardigrade-like life &mdash; which is to say, "
    "the universe may be <b>far more inhabited</b> than optimistic estimates assume.</p>";

static const char WA48[] =
    "<p>Beneath nearly every forest floor lies an invisible network of extraordinary complexity: the "
    "<b>mycelium</b> of fungi, a web of thread-like hyphae extending through soil, wood, and root systems "
    "across vast distances. The mycelium of a single fungal organism can cover enormous areas; the largest "
    "known individual organism on Earth is a specimen of <b>Armillaria ostoyae</b> (honey fungus) in "
    "Oregon&#39;s Malheur National Forest, covering approximately <b>2,385 acres</b> (965 hectares) and "
    "estimated to be between 2,400 and 8,650 years old &mdash; its true age uncertain because it does not "
    "die in the conventional sense.</p>"
    "<p>Mycelium forms <b>mycorrhizal associations</b> with the roots of approximately 90% of all plant "
    "species in a mutually beneficial relationship: the fungus receives sugars and carbohydrates from the "
    "plant&#39;s photosynthesis; the plant receives water, phosphorus, nitrogen, and other minerals that "
    "the fungus&#39; extensive network can extract from soil far beyond the reach of the plant&#39;s own "
    "roots. Forest ecologist <b>Suzanne Simard</b> demonstrated that trees communicate and share resources "
    "through these networks &mdash; the &ldquo;<b>Wood Wide Web</b>&rdquo; &mdash; with older &ldquo;mother "
    "trees&rdquo; preferentially channeling carbon to seedlings of their own species, even when separated "
    "by meters of soil.</p>"
    "<p>When a tree is stressed by drought, pest attack, or disease, it can <b>broadcast chemical signals</b> "
    "through the mycelial network, prompting connected trees to upregulate their own defenses before the "
    "threat arrives. The network is not neutral infrastructure but an active participant in forest ecology, "
    "influencing competition, cooperation, and succession in ways ecologists are still mapping.</p>"
    "<p>The architecture of mycelial networks is strikingly similar, mathematically, to the <b>internet</b>, "
    "to neural networks, and to the <b>cosmic web</b> of dark matter filaments that structures the large-"
    "scale universe. Whether this convergence reflects a universal optimization principle for information "
    "and resource transport, or is simply a coincidence of branching geometry, remains an open and "
    "genuinely beautiful question.</p>";

static const char WA49[] =
    "<p><b>Bioluminescence</b> is the production of light by living organisms through a chemical reaction. "
    "The core reaction involves a light-emitting compound called <b>luciferin</b> being oxidized in the "
    "presence of an enzyme called <b>luciferase</b>, releasing energy as photons rather than heat &mdash; "
    "hence the characterization as &ldquo;cold light.&rdquo; The reaction is extraordinarily efficient: "
    "fireflies, for example, convert roughly 90% of chemical energy to light, compared to about 10% for "
    "incandescent bulbs.</p>"
    "<p>Bioluminescence has <b>evolved independently approximately 40 to 50 times</b> in separate lineages "
    "&mdash; in bacteria, dinoflagellates, fungi, jellyfish, crustaceans, insects, fish, and others &mdash; "
    "making it one of the most striking examples of <b>convergent evolution</b> in biology. In the deep "
    "ocean, where no sunlight penetrates, approximately <b>76% of species</b> produce bioluminescence, "
    "making it the most common form of communication on Earth by species count. Uses include: the anglerfish&#39;s "
    "bioluminescent lure for predation; <b>counter-illumination camouflage</b> in which squid match the "
    "downwelling light to eliminate their shadow; <b>firefly mating signals</b> (each species has a unique "
    "flash pattern); and the blue bioluminescent flash of disturbed dinoflagellates, which may deter "
    "predators by attracting larger predators to the disturbance.</p>"
    "<p>The scientific application of bioluminescence has been transformative. <b>Green fluorescent protein "
    "(GFP)</b>, isolated from the jellyfish <i>Aequorea victoria</i>, won the 2008 Nobel Prize in Chemistry "
    "for Osamu Shimomura, Martin Chalfie, and Roger Tsien. By genetically fusing GFP to proteins of interest, "
    "researchers can track the location and movement of specific molecules in living cells in real time &mdash; "
    "revolutionizing cell biology, neuroscience, and drug development.</p>"
    "<p>Bioluminescent organisms have also inspired <b>medical imaging</b> (luciferase-based reporters detect "
    "tumors in live animals), biosensor technologies (bacteria engineered to glow in the presence of "
    "pollutants), and efforts to develop <b>bioluminescent plants</b> as low-energy ambient lighting. "
    "The phenomenon that evolved as a survival mechanism in the ancient ocean is becoming a versatile "
    "tool at the intersection of biology, chemistry, and technology.</p>";

static const char WA50[] =
    "<p>The standard model of genetic inheritance &mdash; genes passing <b>vertically</b> from parent to "
    "offspring down the generations &mdash; is not the whole story. <b>Horizontal gene transfer (HGT)</b> "
    "is the movement of genetic material directly between organisms that are not in a parent-offspring "
    "relationship, including between entirely different species and even different kingdoms of life. In "
    "bacteria, HGT is so pervasive that evolutionary biologists now describe the bacterial world not as "
    "a tree of life but as a <b>web</b>, with genes flowing laterally in all directions.</p>"
    "<p>HGT is the primary engine driving the spread of <b>antibiotic resistance</b>. Bacteria share "
    "resistance genes across species boundaries via plasmids (small circular DNA molecules that can be "
    "transferred between cells), transduction (bacteriophage viruses that carry DNA between hosts), and "
    "direct uptake of environmental DNA. A resistance gene that evolved in one species can spread to "
    "entirely unrelated pathogens within years &mdash; one reason antibiotic resistance has spread globally "
    "so rapidly since the antibiotic era began.</p>"
    "<p>HGT has also shaped the human genome in profound ways. Approximately <b>8% of the human genome</b> "
    "consists of sequences derived from ancient <b>retroviruses</b> that infected our ancestors and integrated "
    "their genomes into ours. Much of this was long dismissed as &ldquo;junk DNA,&rdquo; but some has been "
    "co-opted for essential functions. The <b>syncytin genes</b> &mdash; derived from viral envelope proteins "
    "that integrated into mammalian genomes approximately 130 million years ago &mdash; are now required for "
    "the formation of the <b>placenta</b> in mammals. Human pregnancy is, in part, made possible by a "
    "repurposed viral gene.</p>"
    "<p><b>Tardigrades</b> have acquired roughly 6,000 genes from bacteria, fungi, and plants through HGT &mdash; "
    "a higher proportion of foreign genes than any other animal known, possibly contributing to their "
    "extraordinary stress resistance. HGT blurs the boundaries between species and challenges the concept "
    "of individual organismal identity. Life does not merely inherit from ancestors &mdash; it is in "
    "constant <b>genetic conversation</b> with the living world around it.</p>";

static const char WA51[] =
    "<p><b>Extremophiles</b> are organisms that thrive in environmental conditions lethal to most life. "
    "Their existence has radically expanded our understanding of the boundaries of the habitable zone &mdash; "
    "for Earth and for the universe. <b>Thermophiles</b> grow optimally at temperatures above 60&deg;C; "
    "<b>hyperthermophiles</b> prefer above 80&deg;C. The record holder, <i>Pyrolobus fumarii</i>, grows "
    "optimally at <b>113&deg;C</b> and cannot grow below 90&deg;C. It was discovered living in hydrothermal "
    "vents on the ocean floor, where water remains liquid only because of the immense pressure.</p>"
    "<p>At the other extreme, <b>psychrophiles</b> thrive in Antarctic ice, with some algae photosynthesizing "
    "at -20&deg;C inside ice crystals. <b>Acidophiles</b> inhabit environments with pH near 0 (the acidity "
    "of battery acid); <b>alkaliphiles</b> live at pH 12. <b>Halophiles</b> flourish in the Dead Sea and "
    "salt lakes up to ten times saltier than seawater. <b>Piezophiles</b> live at the crushing pressures "
    "of deep ocean trenches &mdash; over 1,100 atmospheres at the bottom of the Challenger Deep. Some "
    "microbes have been revived from 250-million-year-old salt crystals.</p>"
    "<p>Perhaps most remarkable are the <b>radiotrophic fungi</b> discovered in 1991 growing on the walls "
    "of the destroyed Chernobyl nuclear reactor, in an environment of intense ionizing radiation that "
    "kills most organisms. These fungi contain high concentrations of <b>melanin</b> and appear to use "
    "ionizing radiation as an energy source for growth &mdash; a form of &ldquo;radiosynthesis&rdquo; "
    "analogous to photosynthesis. The mechanism remains incompletely understood.</p>"
    "<p>The discovery of extremophiles transformed <b>astrobiology</b>. If life can thrive at 113&deg;C, "
    "at pH 0, under 1,100 atmospheres, within nuclear reactors, and in the vacuum of space (tardigrades), "
    "then the habitable zones of the cosmos are vastly larger than models built around human-comfortable "
    "conditions would predict. Jupiter&#39;s moon <b>Europa</b>, with its liquid ocean beneath an ice "
    "shell, and Saturn&#39;s moon <b>Enceladus</b>, with its hydrothermal vents actively venting water "
    "into space, became prime astrobiology targets precisely because extremophiles showed us that "
    "hydrothermal chemistry in cold, pressurized, radiation-bathed environments is not a barrier to "
    "life &mdash; it may be life&#39;s preferred address.</p>";

static const WikiArticle WIKI[] = {
    {"Black Holes",                         0, WA0},
    {"Dark Matter &amp; Dark Energy",        0, WA1},
    {"Neutron Stars &amp; Pulsars",          0, WA2},
    {"Cosmic Microwave Background",          0, WA3},
    {"The Fermi Paradox",                    0, WA4},
    {"The Observable Universe",              0, WA5},
    {"Wave-Particle Duality",                1, WA6},
    {"Quantum Entanglement",                 1, WA7},
    {"The Uncertainty Principle",            1, WA8},
    {"Many-Worlds Interpretation",           1, WA9},
    {"Quantum Tunneling",                    1, WA10},
    {"Schr&ouml;dinger&#39;s Cat",           1, WA11},
    {"The Hard Problem of Consciousness",    2, WA12},
    {"Integrated Information Theory",        2, WA13},
    {"Panpsychism",                          2, WA14},
    {"Lucid Dreaming",                       2, WA15},
    {"The Default Mode Network",             2, WA16},
    {"Psilocybin Mushrooms",                 3, WA17},
    {"Ayahuasca &amp; DMT",                  3, WA18},
    {"Cannabis &amp; the Endocannabinoid System", 3, WA19},
    {"Peyote &amp; Mescaline",               3, WA20},
    {"Iboga &amp; Ibogaine",                 3, WA21},
    {"The Mandela Effect",                   4, WA22},
    {"Synchronicity",                        4, WA23},
    {"Near-Death Experiences",               4, WA24},
    {"Ball Lightning",                       4, WA25},
    {"The Wow! Signal",                      4, WA26},
    {"Time Dilation",                           5, WA27},
    {"The Multiverse",                          5, WA28},
    {"The Simulation Hypothesis",               5, WA29},
    {"The Anthropic Principle",                 5, WA30},
    {"Boltzmann Brains",                        5, WA31},
    {"The Antikythera Mechanism",               6, WA32},
    {"Nikola Tesla",                            6, WA33},
    {"The Dancing Plague of 1518",              6, WA34},
    {"Jack Parsons",                            6, WA35},
    {"The Voynich Manuscript",                  6, WA36},
    {"The Higgs Boson",                         7, WA37},
    {"Acoustic Levitation",                     7, WA38},
    {"CERN &amp; the LHC",                      7, WA39},
    {"Cymatics",                                7, WA40},
    {"Quantum Computing",                       7, WA41},
    {"Sacred Geometry",                         8, WA42},
    {"Alchemy",                                 8, WA43},
    {"The Hermetic Principles",                 8, WA44},
    {"Numerology &amp; Mathematical Mysticism", 8, WA45},
    {"Astrology&#39;s Astronomical Roots",      8, WA46},
    {"Tardigrades",                             9, WA47},
    {"Mycelial Networks",                       9, WA48},
    {"Bioluminescence",                         9, WA49},
    {"Horizontal Gene Transfer",                9, WA50},
    {"Extremophiles",                           9, WA51},
};
static const uint8_t WIKI_COUNT = 52;

static const char WIKI_CSS[] =
    "*{margin:0;padding:0;box-sizing:border-box}"
    "body{background:radial-gradient(ellipse at 50% 50%,#0d003d,#000010 70%);"
    "min-height:100vh;display:flex;flex-direction:column;align-items:center;"
    "font-family:'Courier New',monospace;color:#fff;padding:24px 16px}"
    "nav{font-size:.45rem;letter-spacing:3px;margin-bottom:10px;align-self:flex-start}"
    "nav a{color:rgba(199,119,255,.7);text-decoration:none;"
    "padding:4px 8px;border:1px solid rgba(199,119,255,.25);border-radius:5px}"
    "h1{font-size:1.1rem;letter-spacing:6px;text-align:center;margin:12px 0 4px;"
    "background:linear-gradient(90deg,#c77dff,#8338ec,#c77dff);"
    "-webkit-background-clip:text;-webkit-text-fill-color:transparent}"
    ".sub{font-size:.42rem;letter-spacing:6px;color:rgba(199,119,255,.35);"
    "margin-bottom:20px;text-align:center}"
    ".cat-card{display:block;width:min(440px,94vw);margin-bottom:9px;padding:13px 16px;"
    "background:rgba(20,0,50,.7);border:1px solid rgba(131,56,236,.45);border-radius:10px;"
    "text-decoration:none;color:#fff;transition:border-color .15s}"
    ".cat-card:hover{border-color:rgba(199,119,255,.9)}"
    ".cat-title{font-size:.65rem;letter-spacing:3px}"
    ".cat-count{font-size:.38rem;letter-spacing:3px;color:rgba(199,119,255,.45);margin-top:3px}"
    ".art-item{display:block;width:min(440px,94vw);margin-bottom:8px;padding:12px 14px;"
    "background:rgba(15,0,45,.7);border:1px solid rgba(131,56,236,.4);border-radius:9px;"
    "text-decoration:none;color:#fff;transition:border-color .15s}"
    ".art-item:hover{border-color:rgba(199,119,255,.85)}"
    ".art-title{font-size:.6rem;letter-spacing:2px;color:#c77dff}"
    ".art-prev{font-size:.38rem;letter-spacing:.5px;color:rgba(255,255,255,.4);"
    "margin-top:4px;line-height:1.5}"
    ".body p{line-height:1.7;font-size:.55rem;margin-bottom:12px;color:rgba(255,255,255,.85)}"
    ".body b{color:#c77dff}"
    ".back{display:inline-block;margin-top:18px;font-size:.45rem;letter-spacing:3px;"
    "color:rgba(199,119,255,.6);text-decoration:none;padding:6px 12px;"
    "border:1px solid rgba(131,56,236,.35);border-radius:6px}";




void handleWiki() {
    String html;
    html.reserve(3200);
    html += F("<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>"
              "<meta name='viewport' content='width=device-width,initial-scale=1'>"
              "<title>COSMIC WIKI</title><style>");
    html += WIKI_CSS;
    html += F("</style></head><body>"
              "<nav><a href='/'>&#x2190; HOME</a></nav>"
              "<h1>COSMIC WIKI</h1>"
              "<p class='sub'>52 ARTICLES &middot; 10 CATEGORIES &middot; KNOWLEDGE FROM THE VOID</p>");
    for (uint8_t c = 0; c < WIKI_CAT_COUNT; c++) {
        uint8_t cnt = WIKI_CAT_SIZES[c];
        html += F("<a class='cat-card' href='/wiki/cat?c=");
        html += c;
        html += F("'><div class='cat-title'>");
        html += WIKI_CATS[c];
        html += F("</div><div class='cat-count'>");
        html += cnt;
        html += F(" ARTICLES &rarr;</div></a>");
    }
    html += F("</body></html>");
    cpServer->send(200, "text/html", html);
}

void handleWikiCat() {
    int c = cpServer->arg("c").toInt();
    if (c < 0 || c >= WIKI_CAT_COUNT) { cpServer->send(404, "text/plain", "Not found"); return; }
    String html;
    html.reserve(4000);
    html += F("<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>"
              "<meta name='viewport' content='width=device-width,initial-scale=1'>"
              "<title>COSMIC WIKI</title><style>");
    html += WIKI_CSS;
    html += F("</style></head><body>"
              "<nav><a href='/wiki'>&#x2190; WIKI</a></nav>"
              "<h1>COSMIC WIKI</h1><p class='sub'>");
    html += WIKI_CATS[c];
    html += F("</p>");
    for (uint8_t i = 0; i < WIKI_COUNT; i++) {
        if (WIKI[i].cat != c) continue;
        // preview: first 80 chars of body text (skip HTML tags)
        char preview[84]; uint8_t pi = 0; bool inTag = false;
        const char* b = WIKI[i].body;
        while (*b && pi < 80) {
            if (*b == '<') { inTag = true; b++; continue; }
            if (*b == '>') { inTag = false; b++; continue; }
            if (!inTag) { preview[pi++] = *b; }
            b++;
        }
        preview[pi] = 0;
        html += F("<a class='art-item' href='/wiki/read?id=");
        html += i;
        html += F("'><div class='art-title'>");
        html += WIKI[i].title;
        html += F("</div><div class='art-prev'>");
        html += preview;
        html += F("&hellip;</div></a>");
    }
    html += F("<a class='back' href='/wiki'>&#x2190; ALL CATEGORIES</a></body></html>");
    cpServer->send(200, "text/html", html);
}

void handleWikiRead() {
    int id = cpServer->arg("id").toInt();
    if (id < 0 || id >= WIKI_COUNT) { cpServer->send(404, "text/plain", "Not found"); return; }
    const WikiArticle& art = WIKI[id];
    String html;
    html.reserve(5000);
    html += F("<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>"
              "<meta name='viewport' content='width=device-width,initial-scale=1'>"
              "<title>");
    html += art.title;
    html += F("</title><style>");
    html += WIKI_CSS;
    html += F("</style></head><body>"
              "<nav><a href='/wiki/cat?c=");
    html += art.cat;
    html += F("'>&#x2190; ");
    html += WIKI_CATS[art.cat];
    html += F("</a></nav><h1>");
    html += art.title;
    html += F("</h1><p class='sub'>");
    html += WIKI_CATS[art.cat];
    html += F("</p><div class='body' style='width:min(440px,94vw)'>");
    html += art.body;
    html += F("</div><a class='back' href='/wiki'>&#x2190; ALL CATEGORIES</a></body></html>");
    cpServer->send(200, "text/html", html);
}


// ═══════════════════════════════════════════════════════════════════════════
//  Set AP Name
// ═══════════════════════════════════════════════════════════════════════════



// ═══════════════════════════════════════════════════════════════════════════
//  Visitor Guestbook
// ═══════════════════════════════════════════════════════════════════════════


static String gbSanitize(String s, int maxLen) {
    String out = "";
    for (int i = 0; i < (int)s.length() && (int)out.length() < maxLen; i++) {
        char c = s[i];
        if (c >= 0x20 && c != '<' && c != '>' && c != '&') out += c;
    }
    return out;
}

static void gbReadEntries(std::vector<String>& lines, int maxLines) {
    // Use Preferences for guestbook storage (no SD dependency)
        Preferences p;
    p.begin("cosmic-gb", true);
    String gbContent = p.getString("entries", "");
    p.end();
    // Split by newline
    int start = 0;
    for (int i = 0; i <= (int)gbContent.length(); i++) {
        if (i == (int)gbContent.length() || gbContent[i] == '\n') {
            String line = gbContent.substring(start, i);
            line.trim();
            if (line.length() > 1) lines.push_back(line);
            start = i + 1;
        }
    }
    // Keep only last maxLines
    if ((int)lines.size() > maxLines) {
        lines.erase(lines.begin(), lines.begin() + (lines.size() - maxLines));
    }
}

void handleGuestbook() {
    std::vector<String> entries;
    gbReadEntries(entries, 10);

    static const char* GB_CSS =
        "*{margin:0;padding:0;box-sizing:border-box}"
        "body{background:radial-gradient(ellipse at 50% 50%,#0d003d,#000010 70%);"
        "min-height:100vh;display:flex;flex-direction:column;align-items:center;"
        "font-family:'Courier New',monospace;color:#fff;padding:24px 16px}"
        "nav a{font-size:.5rem;letter-spacing:4px;color:rgba(199,119,255,.55);"
        "text-decoration:none;padding:5px 10px;border:1px solid rgba(199,119,255,.25);border-radius:6px}"
        "h1{font-size:1.1rem;letter-spacing:6px;text-align:center;margin:18px 0 4px;"
        "background:linear-gradient(90deg,#c77dff,#8338ec,#c77dff);"
        "-webkit-background-clip:text;-webkit-text-fill-color:transparent}"
        ".sub{font-size:.42rem;letter-spacing:6px;color:rgba(199,119,255,.35);margin-bottom:20px;text-align:center}"
        ".box{width:min(420px,94vw);background:rgba(20,0,40,.75);"
        "border:1px solid rgba(131,56,236,.4);border-radius:12px;padding:20px;margin-bottom:14px}"
        ".entry{padding:10px 0;border-bottom:1px solid rgba(131,56,236,.2)}"
        ".entry:last-child{border-bottom:none}"
        ".ename{font-size:.48rem;letter-spacing:3px;color:#c77dff;margin-bottom:4px}"
        ".emsg{font-size:.52rem;letter-spacing:1px;color:rgba(224,208,255,.85);line-height:1.5}"
        ".empty{font-size:.45rem;letter-spacing:3px;color:rgba(199,119,255,.3);text-align:center;padding:14px 0}"
        "input,textarea{width:100%;padding:9px;background:rgba(0,0,0,.4);"
        "border:1px solid rgba(131,56,236,.4);border-radius:6px;color:#c77dff;"
        "font-family:'Courier New',monospace;font-size:.5rem;margin-top:8px;resize:none}"
        ".lbl{font-size:.42rem;letter-spacing:3px;color:rgba(199,119,255,.5);display:block;margin-top:12px}"
        ".btn{display:block;width:100%;padding:12px;margin-top:14px;"
        "background:rgba(131,56,236,.25);border:1px solid rgba(131,56,236,.6);"
        "border-radius:8px;color:#c77dff;font-family:'Courier New',monospace;"
        "font-size:.55rem;letter-spacing:4px;cursor:pointer;text-align:center}"
        ".hint{font-size:.38rem;letter-spacing:2px;color:rgba(255,255,255,.18);margin-top:8px;text-align:center}";

    String html = F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>GUESTBOOK</title><style>");
    html += GB_CSS;
    html += F("</style></head><body>"
        "<nav><a href='/'>&#x2190; BACK</a></nav>"
        "<h1>COSMIC GUESTBOOK</h1><p class='sub'>LEAVE YOUR MARK ON THE UNIVERSE</p>");

    // Entries
    html += F("<div class='box'>");
    if (entries.empty()) {
        html += F("<p class='empty'>NO ENTRIES YET \xe2\x80\x94 BE THE FIRST</p>");
    } else {
        for (int i = (int)entries.size() - 1; i >= 0; i--) {
            String line = entries[i];
            int sep = line.indexOf('\x01');
            String name = (sep >= 0) ? line.substring(0, sep) : "ANONYMOUS";
            String msg  = (sep >= 0) ? line.substring(sep + 1) : line;
            html += F("<div class='entry'><div class='ename'>");
            html += (name.length() > 0 ? name : "ANONYMOUS");
            html += F("</div><div class='emsg'>");
            html += msg;
            html += F("</div></div>");
        }
    }
    html += F("</div>");

    // Sign form
    html += F("<div class='box'>"
            "<form method='POST' action='/guestbook/sign'>"
            "<span class='lbl'>YOUR NAME (optional)</span>"
            "<input type='text' name='name' maxlength='20' placeholder='TRAVELER...'>"
            "<span class='lbl'>YOUR MESSAGE</span>"
            "<textarea name='msg' maxlength='100' rows='3' placeholder='Leave a trace in the cosmos...' required></textarea>"
            "<button type='submit' class='btn'>&#x2B50; SIGN THE COSMOS</button>"
            "</form>"
            "<p class='hint'>Messages appear to all future visitors</p>"
            "</div>");
    html += F("</body></html>");
    cpServer->send(200, "text/html", html);
}

void handleGuestbookSign() {
    String name = gbSanitize(cpServer->arg("name"), 20);
    String msg  = gbSanitize(cpServer->arg("msg"),  100);
    if (msg.length() == 0) {
        cpServer->sendHeader("Location", "/guestbook", true);
        cpServer->send(302, "text/plain", "");
        return;
    }
    // Store in Preferences
    Preferences p;
    p.begin("cosmic-gb", false);
    String existing = p.getString("entries", "");
    if (existing.length() + name.length() + msg.length() + 4 > 7000) {
        // Trim oldest entries to stay under limit
        int cut = existing.indexOf('\n');
        if (cut > 0) existing = existing.substring(cut + 1);
    }
    existing += name + "\x01" + msg + "\n";
    p.putString("entries", existing);
    p.end();
    cpServer->sendHeader("Location", "/guestbook", true);
    cpServer->send(302, "text/plain", "");
}


void handleApiVisitorMsg() {
    if (cpServer->method() != HTTP_POST) {
        cpServer->send(405, "text/plain", "Method Not Allowed");
        return;
    }
    String body = cpServer->arg("plain");
    body.trim();
    if (body.length() == 0) { cpServer->send(400, "text/plain", "Empty message"); return; }
    if (body.length() > 50) body = body.substring(0, 50);
    cpVisitorMsg   = body;
    cpVisitorMsgAt = millis();
    cpShowVisMsg   = true;
    cpServer->send(200, "text/plain", "OK");
}


// ═══════════════════════════════════════════════════════════════════════════
//  Set AP Name Handler
// ═══════════════════════════════════════════════════════════════════════════

void handleSetAP() {
    if (cpServer->method() == HTTP_POST) {
        String newName = cpServer->arg("apname");
        newName.trim();
        if (newName.length() == 0) {
            cpApName = "";
            cpPrefs.remove("cpApName");
        } else {
            if (newName.length() > 32) newName = newName.substring(0, 32);
            cpApName = newName;
            cpPrefs.putString("cpApName", cpApName);
        }
        cpPrefs.end();
        cpPrefs.begin("cosmic", false);
        WiFi.softAPdisconnect(false);
        delay(100);
        WiFi.softAP(cpApName.length() > 0 ? cpApName.c_str() : CP_AP_SSID);
        cpServer->sendHeader("Location", "/setap?msg=saved", true);
        cpServer->send(302, "", "");
        return;
    }

    String msg = cpServer->arg("msg");
    String cur = cpApName.length() > 0 ? cpApName : String(CP_AP_SSID);

    String html = F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>SET AP NAME</title><style>");
    html += SETAP_CSS;
    html += F("</style></head><body>"
        "<nav><a href='/'>&#x2190; BACK</a></nav>"
        "<h1>SET AP NAME</h1>"
        "<p class='sub'>RENAME YOUR PORTAL HOTSPOT</p>");
    if (msg == "saved") {
        html += F("<p style='text-align:center;color:#6f6;font-size:.45rem;"
                  "letter-spacing:3px;margin-bottom:14px'>&#x2713; SAVED &amp; APPLIED</p>");
    }
    html += F("<div class='box'>"
        "<label>PORTAL NAME (max 32 chars)</label>"
        "<input type='text' id='n' name='apname' maxlength='32' value='");
    html += cur;
    html += F("'><label>COSMIC EMOJIS &mdash; TAP TO APPEND</label>"
        "<div class='emojis'>");
    static const char* const EMOJIS[] = {
        "\xF0\x9F\x8C\x8C","\xF0\x9F\xAA\x90","\xF0\x9F\x8C\xA0",
        "\xF0\x9F\x8C\x9F","\xE2\xAD\x90","\xF0\x9F\x92\xAB",
        "\xE2\x9C\xA8","\xF0\x9F\x8C\x99","\xE2\x98\x80",
        "\xF0\x9F\x94\xAE","\xF0\x9F\x91\x81","\xE2\x9A\xA1",
        "\xF0\x9F\x94\xA5","\xF0\x9F\x8C\x80","\xF0\x9F\x8C\x8A",
        "\xF0\x9F\x9B\xB8","\xF0\x9F\xA7\xAC","\xE2\x98\xAF",
        "\xF0\x9F\x92\x8E","\xF0\x9F\x92\x80","\xE2\x9D\xA4","\xF0\x9F\x8E\xA8",
    };
    for (int i = 0; i < 22; i++) {
        html += F("<button type='button' onclick=\"document.getElementById('n').value+='");
        html += EMOJIS[i];
        html += F("'\">");
        html += EMOJIS[i];
        html += F("</button>");
    }
    html += F("</div>"
        "<form method='POST' action='/setap'>"
        "<input type='hidden' id='hid' name='apname' value=''>"
        "<div class='row'>"
        "<button class='btn save' type='button' onclick=\""
            "document.getElementById('hid').value=document.getElementById('n').value;"
            "this.closest('form').submit()\">&#x1F4BE; SAVE &amp; APPLY</button>"
        "<button class='btn reset' type='button' onclick=\""
            "document.getElementById('n').value='';"
            "document.getElementById('hid').value='';"
            "this.closest('form').submit()\">&#x21BA; RESET DEFAULT</button>"
        "</div></form>"
        "<p class='note'>&#x26A0; ALL CONNECTED VISITORS WILL BRIEFLY DISCONNECT WHEN SAVED</p>"
        "</div></body></html>");
    cpServer->send(200, "text/html", html);
}


// ═══════════════════════════════════════════════════════════════════════════
//  Portal Lifecycle
// ═══════════════════════════════════════════════════════════════════════════


static void cpHandleRedirect() {
    cpServer->sendHeader("Location", "http://" CP_PORTAL_IP "/", true);
    cpServer->send(302, "text/plain", "");
}

static void cpInitPortal() {
    // Load saved AP name
    cpPrefs.begin("cosmic", true);
    cpApName = cpPrefs.getString("cpApName", "");
    cpPrefs.end();
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP(cpApName.length() > 0 ? cpApName.c_str() : CP_AP_SSID);
    delay(200);

    cpDnsServer = new DNSServer();
    cpServer = new WebServer(80);

    IPAddress apIP = WiFi.softAPIP();
    cpDnsServer->start(CP_DNS_PORT, "*", apIP);

    // ── Art modes ─────────────────────────────────────────────────────────
    cpServer->on("/",             HTTP_GET,  handleIndex);
    cpServer->on("/safety",       HTTP_GET,  handleSafety);
    cpServer->on("/mandala",      HTTP_GET,  handleMandala);
    cpServer->on("/plasma",       HTTP_GET,  handlePlasma);
    cpServer->on("/fractal",      HTTP_GET,  handleFractal);
    cpServer->on("/matrix",       HTTP_GET,  handleMatrix);
    cpServer->on("/cyber",        HTTP_GET,  handleCyber);
    cpServer->on("/binary",       HTTP_GET,  handleBinary);
    cpServer->on("/starfield",    HTTP_GET,  handleStarfield);
    cpServer->on("/particles",    HTTP_GET,  handleParticles);
    cpServer->on("/tunnel",       HTTP_GET,  handleTunnel);
    cpServer->on("/mfire",        HTTP_GET,  handleMfire);
    cpServer->on("/mice",         HTTP_GET,  handleMice);
    cpServer->on("/mstorm",       HTTP_GET,  handleMstorm);
    cpServer->on("/mblood",       HTTP_GET,  handleMblood);
    cpServer->on("/mgold",        HTTP_GET,  handleMgold);
    cpServer->on("/mvoid",        HTTP_GET,  handleMvoid);
    cpServer->on("/mphantom",     HTTP_GET,  handleMphantom);
    cpServer->on("/mripple",      HTTP_GET,  handleMripple);
    cpServer->on("/mglitch",      HTTP_GET,  handleMglitch);
    cpServer->on("/hopalong",     HTTP_GET,  handleHopalong);
    cpServer->on("/interference", HTTP_GET,  handleInterference);
    cpServer->on("/voronoi",      HTTP_GET,  handleVoronoi);
    cpServer->on("/strange",      HTTP_GET,  handleStrange);
    cpServer->on("/lissajous",    HTTP_GET,  handleLissajous);
    cpServer->on("/sierpinski",   HTTP_GET,  handleSierpinski);
    cpServer->on("/spirograph",   HTTP_GET,  handleSpirograph);
    cpServer->on("/barnsley",     HTTP_GET,  handleBarnsley);
    cpServer->on("/campfire",     HTTP_GET,  handleCampfire);
    cpServer->on("/raindrops",    HTTP_GET,  handleRaindrops);
    cpServer->on("/gameoflife",   HTTP_GET,  handleGameoflife);
    cpServer->on("/aurora",       HTTP_GET,  handleAurora);
    cpServer->on("/kaleidoscope", HTTP_GET,  handleKaleidoscope);
    cpServer->on("/dragon",       HTTP_GET,  handleDragon);
    cpServer->on("/lava2",        HTTP_GET,  handleLava2);
    cpServer->on("/noise",        HTTP_GET,  handleNoise);
    cpServer->on("/snake",        HTTP_GET,  handleSnake);
    cpServer->on("/breakout",     HTTP_GET,  handleBreakout);
    cpServer->on("/tetris",       HTTP_GET,  handleTetris);
    cpServer->on("/dodge",        HTTP_GET,  handleDodge);
    cpServer->on("/asteroids",    HTTP_GET,  handleAsteroids);
    cpServer->on("/apollonian",   HTTP_GET,  handleApollonian);
    cpServer->on("/sunflower",    HTTP_GET,  handleSunflower);
    cpServer->on("/quasicrystal", HTTP_GET,  handleQuasicrystal);
    cpServer->on("/lorenz",       HTTP_GET,  handleLorenz);
    cpServer->on("/mandelbrot",   HTTP_GET,  handleMandelbrot);
    cpServer->on("/reaction",     HTTP_GET,  handleReaction);
    cpServer->on("/maze",         HTTP_GET,  handleMaze);
    cpServer->on("/vines",        HTTP_GET,  handleVines);
    cpServer->on("/snowflakes",   HTTP_GET,  handleSnowflakes);
    cpServer->on("/cube3d",       HTTP_GET,  handleCube3d);
    cpServer->on("/torus",        HTTP_GET,  handleTorus);
    cpServer->on("/hypercube",    HTTP_GET,  handleHypercube);
    cpServer->on("/retrogeo",     HTTP_GET,  handleRetrogeo);
    cpServer->on("/mirrorblob",   HTTP_GET,  handleMirrorblob);
    cpServer->on("/cityflow",     HTTP_GET,  handleCityflow);
    cpServer->on("/fireworks",    HTTP_GET,  handleFireworks);
    cpServer->on("/coral",        HTTP_GET,  handleCoral);
    cpServer->on("/cwaves",       HTTP_GET,  handleCwaves);
    cpServer->on("/deepstars",    HTTP_GET,  handleDeepstars);
    cpServer->on("/flowfield",    HTTP_GET,  handleFlowfield);
    cpServer->on("/metaballs",    HTTP_GET,  handleMetaballs);
    cpServer->on("/goop",         HTTP_GET,  handleGoop);
    cpServer->on("/wormhole",     HTTP_GET,  handleWormhole);
    cpServer->on("/crystal",      HTTP_GET,  handleCrystal);
    cpServer->on("/lightning",    HTTP_GET,  handleLightning);
    cpServer->on("/bounceballs",  HTTP_GET,  handleBounceballs);
    cpServer->on("/neonrain",     HTTP_GET,  handleNeonrain);
    cpServer->on("/dna",          HTTP_GET,  handleDna);
    cpServer->on("/sandfall",     HTTP_GET,  handleSandfall);
    cpServer->on("/acidspiral",   HTTP_GET,  handleAcidspiral);
    cpServer->on("/plasmaglobe",  HTTP_GET,  handlePlasmaglobe);
    cpServer->on("/warpgrid",     HTTP_GET,  handleWarpgrid);
    cpServer->on("/nebula",       HTTP_GET,  handleNebula);

    // ── Wiki ───────────────────────────────────────────────────────────────
    cpServer->on("/wiki",           HTTP_GET,  handleWiki);
    cpServer->on("/wiki/cat",       HTTP_GET,  handleWikiCat);
    cpServer->on("/wiki/read",      HTTP_GET,  handleWikiRead);

    // ── Guestbook ──────────────────────────────────────────────────────────
    cpServer->on("/guestbook",      HTTP_GET,  handleGuestbook);
    cpServer->on("/guestbook/sign", HTTP_POST, handleGuestbookSign);

    // ── Settings ───────────────────────────────────────────────────────────
    cpServer->on("/setap",          HTTP_GET,  handleSetAP);
    cpServer->on("/setap",          HTTP_POST, handleSetAP);

    // ── API ────────────────────────────────────────────────────────────────
    cpServer->on("/api/msg",          HTTP_GET,  handleApiMsg);
    cpServer->on("/api/visitor-msg",  HTTP_POST, handleApiVisitorMsg);

    cpServer->on("/favicon.ico", HTTP_GET, []() {{ cpServer->send(404, "text/plain", ""); }});
    cpServer->onNotFound(cpHandleRedirect);
    cpServer->begin();

    cpRunning = true;
    GW_LOGLN("[CosmicPortal] Portal started at " CP_PORTAL_IP);
}

static void cpRunPortal() {
    if (!cpRunning) return;
    cpDnsServer->processNextRequest();
    cpServer->handleClient();
}

static void cpClosePortal() {
    if (!cpRunning) return;
    cpServer->stop();
    cpDnsServer->stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(300);
    delete cpServer;
    cpServer = nullptr;
    delete cpDnsServer;
    cpDnsServer = nullptr;
    cpRunning = false;
    GW_LOGLN("[CosmicPortal] Portal stopped.");
}

static bool cpIsRunning() { return cpRunning; }
