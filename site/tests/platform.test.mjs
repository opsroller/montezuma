// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Run: node --test site/tests/
import { test } from "node:test";
import assert from "node:assert/strict";
import { detectPlatform } from "../src/js/platform.js";

const UA = {
  windowsChrome: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36",
  windowsFirefox: "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:131.0) Gecko/20100101 Firefox/131.0",
  macSafari: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.0 Safari/605.1.15",
  linuxFirefox: "Mozilla/5.0 (X11; Linux x86_64; rv:131.0) Gecko/20100101 Firefox/131.0",
  ubuntuFirefox: "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:131.0) Gecko/20100101 Firefox/131.0",
  androidPhone: "Mozilla/5.0 (Linux; Android 14; Pixel 8) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Mobile Safari/537.36",
  androidTablet: "Mozilla/5.0 (Linux; Android 13; SM-X710) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36",
  iphone: "Mozilla/5.0 (iPhone; CPU iPhone OS 18_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.0 Mobile/15E148 Safari/604.1",
  ipadMobileSite: "Mozilla/5.0 (iPad; CPU OS 17_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.6 Mobile/15E148 Safari/604.1",
  chromebook: "Mozilla/5.0 (X11; CrOS x86_64 14541.0.0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36",
};

const cases = [
  ["Windows Chrome", { userAgent: UA.windowsChrome }, "windows"],
  ["Windows Firefox", { userAgent: UA.windowsFirefox }, "windows"],
  ["Mac Safari", { userAgent: UA.macSafari, maxTouchPoints: 0 }, "macos"],
  ["iPad in desktop mode (Mac UA, touch)", { userAgent: UA.macSafari, maxTouchPoints: 5 }, "ios"],
  ["iPad mobile site", { userAgent: UA.ipadMobileSite, maxTouchPoints: 5 }, "ios"],
  ["iPhone", { userAgent: UA.iphone, maxTouchPoints: 5 }, "ios"],
  ["Linux Firefox", { userAgent: UA.linuxFirefox }, "linux"],
  ["Ubuntu Firefox", { userAgent: UA.ubuntuFirefox }, "linux"],
  ["Android phone (UA has Linux)", { userAgent: UA.androidPhone }, "android"],
  ["Android tablet", { userAgent: UA.androidTablet }, "android"],
  ["Chromebook", { userAgent: UA.chromebook }, null],
  ["Client hint Windows beats UA", { userAgent: UA.linuxFirefox, userAgentData: { platform: "Windows" } }, "windows"],
  ["Client hint macOS", { userAgent: UA.macSafari, userAgentData: { platform: "macOS" } }, "macos"],
  ["Client hint Android", { userAgent: UA.androidPhone, userAgentData: { platform: "Android" } }, "android"],
  ["Client hint Linux", { userAgent: UA.linuxFirefox, userAgentData: { platform: "Linux" } }, "linux"],
  ["Client hint Chrome OS", { userAgent: UA.chromebook, userAgentData: { platform: "Chrome OS" } }, null],
  ["Empty client hint falls back to UA", { userAgent: UA.windowsChrome, userAgentData: { platform: "" } }, "windows"],
  ["Unknown UA", { userAgent: "curl/8.0" }, null],
  ["Empty navigator", {}, null],
  ["No navigator", undefined, null],
];

for (const [name, nav, expected] of cases) {
  test(name, () => assert.equal(detectPlatform(nav), expected));
}
