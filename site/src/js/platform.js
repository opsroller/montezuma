// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Guesses the visitor's platform so the download popup can lead with the
// right build. The guess only picks the primary button: every platform is
// always listed, so a wrong guess costs a scroll, never a download.

/** Platform keys, matching `key` in data/releases.json. */
export const PLATFORMS = ["windows", "macos", "linux", "android", "ios"];

/**
 * @param {{ userAgent?: string, maxTouchPoints?: number,
 *           userAgentData?: { platform?: string } }} nav  a Navigator or a
 *   test double with the same fields
 * @returns {"windows"|"macos"|"linux"|"android"|"ios"|null} null when unknown
 */
export function detectPlatform(nav) {
  if (!nav) return null;
  const ua = nav.userAgent || "";
  const hinted = nav.userAgentData && nav.userAgentData.platform;

  if (hinted) {
    switch (hinted) {
      case "Windows": return "windows";
      case "macOS": return "macos";
      case "Android": return "android";
      case "iOS": return "ios";
      case "Linux": return "linux";
      // Chrome OS, unknown hints: fall back to no guess.
      default: return null;
    }
  }

  if (/Android/i.test(ua)) return "android"; // before Linux: Android UAs contain it
  if (/iPhone|iPod|iPad/i.test(ua)) return "ios";
  if (/CrOS/.test(ua)) return null;
  if (/Windows/i.test(ua)) return "windows";
  if (/Macintosh|Mac OS X/i.test(ua)) {
    // iPadOS Safari asks for the desktop site with a Mac user agent.
    return (nav.maxTouchPoints || 0) > 1 ? "ios" : "macos";
  }
  if (/Linux/i.test(ua)) return "linux";
  return null;
}
