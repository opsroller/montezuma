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

window.addEventListener(LEVEL_COMPLETED, () => openDownloads(true));
// First frame on screen: the game is playable now, not just loaded.
window.addEventListener(READY, () => {
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

function say(text) {
  status.textContent = text;
}

function fail(text) {
  progress.hidden = true;
  say(text);
  playButton.hidden = false;
  playButton.textContent = "Try again";
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

function loadScript(src) {
  return new Promise((resolve, reject) => {
    const script = document.createElement("script");
    script.src = src;
    script.onload = resolve;
    script.onerror = () => reject(new Error(`could not load ${src}`));
    document.head.append(script);
  });
}

async function startGodot() {
  if (typeof WebAssembly !== "object") {
    fail("Your browser can't run the demo (no WebAssembly). The downloads below work on every platform.");
    return;
  }
  progress.hidden = false;
  progress.removeAttribute("value");
  say("Loading the intro level…");
  try {
    if (typeof Engine === "undefined") await loadScript("play/montezuma.js");
    /* global Engine */
    if (!Engine.isWebGLAvailable(2)) {
      fail("Your browser can't run the demo (no WebGL 2). The downloads below work on every platform.");
      return;
    }
    const canvas = document.createElement("canvas");
    canvas.id = "game-canvas";
    canvas.tabIndex = 0;
    canvas.setAttribute("aria-label", "Montezuma's intro level");
    stage.replaceChildren(canvas);
    const engine = new Engine({
      executable: "play/montezuma",
      canvas,
      canvasResizePolicy: 0, // the page sizes the canvas (see site.css)
      ensureCrossOriginIsolationHeaders: false, // single-threaded export
      onProgress(current, total) {
        if (total > 0) {
          progress.max = total;
          progress.value = current;
        }
      },
    });
    await engine.preloadFile(`play/${EXTENSION}`, EXTENSION);
    await engine.startGame();
    progress.hidden = true;
    leaveButton.hidden = false;
    say("Starting the intro level…");
    canvas.focus();
  } catch (error) {
    console.error(error);
    fail("The demo didn't load. Check your connection and try again, or download the game below.");
  }
}

playButton.addEventListener("click", () => {
  playButton.hidden = true;
  if (section.dataset.demo === "godot") startGodot();
  else startStub();
});

leaveButton.addEventListener("click", () => {
  document.getElementById("skip-link").focus();
});
