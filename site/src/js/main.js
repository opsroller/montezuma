// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// The page's only script: loads the web demo on request, and opens the
// download popup when the demo reports the intro level won (or the visitor
// skips it). Everything here enhances a page that already works without it.

import { detectPlatform } from "./platform.js";

const LEVEL_COMPLETED = "montezuma:level-completed";
const READY = "montezuma:ready";
// The game's extension: preloaded into Godot's file system under this bare
// name, where the game opens it (contract: Web demo hand-off).
const EXTENSION = "libmontezuma.web.template_release.wasm32.nothreads.wasm";

const dialog = document.getElementById("download-dialog");
const stage = document.getElementById("stage");
const status = document.getElementById("demo-status");
const progress = document.getElementById("demo-progress");
const playButton = document.getElementById("play-button");
const leaveButton = document.getElementById("leave-button");
const section = document.getElementById("play");

// ---- Download popup -------------------------------------------------------

function fillPrimary() {
  const slot = dialog.querySelector("[data-primary]");
  const key = detectPlatform(navigator);
  const item = key && dialog.querySelector(`[data-platform="${key}"]`);
  slot.replaceChildren();
  if (!item) {
    slot.textContent = "Pick your platform below.";
    return;
  }
  const link = item.querySelector("a.get");
  const label = item.dataset.label;
  if (link) {
    const primary = link.cloneNode(true);
    primary.className = "button primary";
    slot.append(primary);
  } else {
    slot.textContent = `Montezuma's for ${label} is coming soon. Every platform is listed below.`;
  }
}

function openDownloads(won) {
  if (typeof dialog.showModal !== "function") {
    location.hash = "#download"; // old browsers: the in-page list
    return;
  }
  dialog.querySelector("[data-won]").hidden = !won;
  dialog.querySelector("[data-skipped]").hidden = won;
  fillPrimary();
  if (!dialog.open) dialog.showModal();
}

// Events from the game carry a versioned detail (contract: Web demo
// hand-off). Only version 1 is understood; anything else is ignored.
function isV1(event) {
  return Boolean(event.detail) && event.detail.v === 1;
}

window.addEventListener(LEVEL_COMPLETED, (event) => {
  const { level, score } = event.detail || {};
  if (isV1(event) && typeof level === "string" && Number.isInteger(score)) openDownloads(true);
});
// First frame on screen: the game is playable now, not just loaded. It can
// arrive before startGame() resolves.
let ready = false;
window.addEventListener(READY, (event) => {
  if (!isV1(event) || typeof event.detail.version !== "string") return;
  ready = true;
  section.dataset.gameVersion = event.detail.version;
  say("Intro level running. Match three or more to clear the board.");
});

document.querySelectorAll("[data-open-downloads]").forEach((el) => {
  el.addEventListener("click", (event) => {
    event.preventDefault();
    openDownloads(false);
  });
});

// The close button submits a method="dialog" form and Escape closes the
// popup natively; clicking the backdrop closes it too.
dialog.addEventListener("click", (event) => {
  if (event.target === dialog) dialog.close();
});

// ---- Demo loading ---------------------------------------------------------

// Without JavaScript the play button would do nothing; it shows once this
// module runs (see site.css).
document.documentElement.classList.add("js");

const poster = stage.querySelector(".poster");
// Godot keeps a failed start in module state, so once its engine has been
// created a retry needs a fresh page.
let retryReloads = false;
// No download progress and no start for this long means a load failed
// somewhere Godot does not report (it leaves startGame() pending).
const STALL_MS = 60_000;
let lastProgress = 0;

function say(text) {
  status.textContent = text;
}

// A load failure: the message, and the poster back in the stage with a
// "Try again" button, so no dead canvas keeps the focus.
function fail(text) {
  progress.hidden = true;
  leaveButton.hidden = true;
  say(text);
  stage.replaceChildren(poster);
  playButton.hidden = false;
  playButton.textContent = "Try again";
  playButton.focus();
}

// The browser can't run the demo at all: say what is missing and point to
// the downloads; retrying would not help.
function unsupported(missing) {
  progress.hidden = true;
  stage.replaceChildren(poster);
  const link = document.createElement("a");
  link.href = "#download";
  link.textContent = "Get the game for your computer or phone";
  link.addEventListener("click", (event) => {
    event.preventDefault();
    openDownloads(false);
  });
  status.replaceChildren(`Your browser can't run the demo (no ${missing}). `, link, ".");
}

function startStub() {
  // Stand-in until the WebAssembly build lands (WP-21): it fires the same
  // event the game will, so the hand-off can be tried end to end.
  const panel = document.getElementById("stub-template").content.cloneNode(true);
  stage.replaceChildren(panel);
  stage.querySelector("[data-stub-win]").addEventListener("click", () => {
    window.dispatchEvent(new CustomEvent(LEVEL_COMPLETED, {
      detail: { v: 1, level: "stub", score: 0 },
    }));
  });
  say("Demo preview loaded.");
}

// Keep the canvas's pixel size equal to its CSS size times the screen's
// pixel ratio (capped at 2 to spare phones); canvasResizePolicy 0 leaves
// sizing to the page, and Godot renders at whatever size the canvas has.
function fitCanvas(canvas) {
  const fit = () => {
    const ratio = Math.min(window.devicePixelRatio || 1, 2);
    canvas.width = Math.max(1, Math.round(canvas.clientWidth * ratio));
    canvas.height = Math.max(1, Math.round(canvas.clientHeight * ratio));
  };
  fit();
  if (typeof ResizeObserver === "function") new ResizeObserver(fit).observe(canvas);
}

// The game takes every key, Tab included, so Shift+Escape hands the
// keyboard back to the page (WCAG 2.1.2). Escape alone is the game's pause.
window.addEventListener("keydown", (event) => {
  const canvas = document.getElementById("game-canvas");
  if (event.key !== "Escape" || !event.shiftKey || !canvas || document.activeElement !== canvas) return;
  event.preventDefault();
  event.stopImmediatePropagation();
  leaveButton.focus();
}, true);

function loadScript(src) {
  return new Promise((resolve, reject) => {
    const script = document.createElement("script");
    script.src = src;
    script.onload = resolve;
    script.onerror = () => reject(new Error(`could not load ${src}`));
    document.head.append(script);
  });
}

// Resolves with the game's start, or rejects once loading has made no
// progress for STALL_MS.
function watch(start) {
  lastProgress = Date.now();
  return new Promise((resolve, reject) => {
    const timer = setInterval(() => {
      if (Date.now() - lastProgress > STALL_MS) {
        clearInterval(timer);
        reject(new Error("loading stalled"));
      }
    }, 1000);
    start.then(resolve, reject).finally(() => clearInterval(timer));
  });
}

async function startGodot() {
  if (typeof WebAssembly !== "object") {
    unsupported("WebAssembly");
    return;
  }
  progress.hidden = false;
  progress.removeAttribute("value");
  say("Loading the intro level…");
  try {
    if (typeof Engine === "undefined") await loadScript("play/montezuma.js");
    /* global Engine */
    if (!Engine.isWebGLAvailable(2)) {
      unsupported("WebGL 2");
      return;
    }
    const canvas = document.createElement("canvas");
    canvas.id = "game-canvas";
    canvas.tabIndex = 0;
    canvas.setAttribute("role", "application");
    canvas.setAttribute("aria-label", "Montezuma's intro level");
    canvas.setAttribute("aria-describedby", "demo-help");
    stage.replaceChildren(canvas);
    fitCanvas(canvas);
    leaveButton.hidden = false;
    retryReloads = true;
    const engine = new Engine({
      executable: "play/montezuma",
      canvas,
      canvasResizePolicy: 0, // the page sizes the canvas (see site.css)
      ensureCrossOriginIsolationHeaders: false, // single-threaded export
      fileSizes: JSON.parse(section.dataset.fileSizes || "{}"), // written by site/build.py
      onProgress(current, total) {
        lastProgress = Date.now();
        if (total > 0) {
          progress.max = total;
          progress.value = current;
        }
      },
    });
    const start = engine.preloadFile(`play/${EXTENSION}`, EXTENSION).then(() => engine.startGame());
    await watch(start);
    progress.hidden = true;
    if (!ready) say("Starting the intro level…");
    canvas.focus();
  } catch (error) {
    console.error(error);
    fail("The demo didn't load. Check your connection and try again, or download the game below.");
  }
}

playButton.addEventListener("click", () => {
  if (retryReloads) {
    location.reload();
    return;
  }
  playButton.hidden = true;
  if (section.dataset.demo === "godot") startGodot();
  else startStub();
});

leaveButton.addEventListener("click", () => {
  document.getElementById("skip-link").focus();
  say("The keyboard is back on the page. Tab back to the game to keep playing.");
});
