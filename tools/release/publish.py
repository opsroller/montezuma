# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Assemble and publish a GitHub release from the packages of one version.

Usage:
  publish.py --tag vX.Y.Z [--only linux,web] [--dry-run] [--out DIR] DIR...

Looks in each DIR (recursively) for montezuma-<X.Y.Z>-<target>.<ext> and its
.sha256, checks every checksum, refuses a missing target or a package of
another version, writes SHA256SUMS and the release notes (changelog.py notes
for the tag, plus the source and checksum footer) into --out (default
dist/release), then runs `gh release create`. With --dry-run it stops before
gh and prints what it would publish.

Release targets, all required unless --only narrows them:
  linux    montezuma-X.Y.Z-linux-x86_64.tar.gz
  windows  montezuma-X.Y.Z-windows-x86_64.zip
  macos    montezuma-X.Y.Z-macos-universal.zip
  web      montezuma-X.Y.Z-web.zip

Only the Python standard library, git and (to publish) gh are needed.
"""

import argparse
import hashlib
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import changelog  # noqa: E402

REPO = "opsroller/montezuma"
TARGETS = {
    "linux": "linux-x86_64.tar.gz",
    "windows": "windows-x86_64.zip",
    "macos": "macos-universal.zip",
    "web": "web.zip",
}

FOOTER = """
### Downloads

Every file has a `.sha256` beside it, and `SHA256SUMS` lists them all;
check a download with `sha256sum -c SHA256SUMS --ignore-missing`.
The macOS app is ad-hoc signed and not notarized.

### Source

Montezuma's is free software under the GNU GPL, version 2 or later. The
source of this release is the tag
[{tag}](https://github.com/{repo}/tree/{tag}); each package's `SOURCE.txt`
names the exact commit and `THIRD-PARTY-NOTICES.txt` the licenses of what
ships with it.
"""


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def find_packages(dirs):
    """{file name: path} of every montezuma-* archive under dirs."""
    found = {}
    for top in dirs:
        for root, _, files in os.walk(top):
            for name in files:
                if name.startswith("montezuma-") and name.endswith((".tar.gz", ".zip")):
                    path = os.path.join(root, name)
                    if name in found and sha256(found[name]) != sha256(path):
                        sys.exit(f"{name}: two different files with this name: {found[name]}, {path}")
                    found[name] = path
    return found


def check_package(path):
    """Exits unless path's .sha256 exists, names it and matches its bytes."""
    name = os.path.basename(path)
    sum_path = path + ".sha256"
    if not os.path.isfile(sum_path):
        sys.exit(f"{name}: no {name}.sha256 beside it")
    with open(sum_path, encoding="utf-8") as f:
        fields = f.read().split()
    if len(fields) != 2 or fields[1] != name:
        sys.exit(f"{name}.sha256: expected '<hash>  {name}'")
    if fields[0] != sha256(path):
        sys.exit(f"{name}: checksum does not match {name}.sha256")
    return fields[0]


def collect(version, targets, dirs):
    """[(path, hash)] of the one package per target, in TARGETS order."""
    found = find_packages(dirs)
    expected = {f"montezuma-{version}-{TARGETS[t]}": t for t in targets}
    stray = sorted(n for n in found if f"-{version}-" not in n)
    if stray:
        sys.exit(f"packages of another version found: {', '.join(stray)}; build from the tag")
    missing = [f"{t} ({n})" for n, t in expected.items() if n not in found]
    if missing:
        sys.exit(f"missing release packages: {', '.join(missing)}")
    return [(found[n], check_package(found[n])) for n in expected]


def notes(tag, repo):
    return changelog.notes(tag, changelog.release_tags()) + FOOTER.format(tag=tag, repo=repo)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--tag", required=True)
    parser.add_argument("--only", help="comma-separated targets (default: all)")
    parser.add_argument("--repo", default=REPO)
    parser.add_argument("--out", default=os.path.join("dist", "release"))
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("dirs", nargs="+")
    args = parser.parse_args(argv)

    m = changelog.TAG_RE.match(args.tag)
    if not m:
        sys.exit(f"{args.tag} is not a release tag (vX.Y.Z)")
    if changelog.git("rev-parse", "-q", "--verify", f"refs/tags/{args.tag}", check=False).returncode != 0:
        sys.exit(f"{args.tag}: no such tag in this repository")
    version = args.tag[1:]
    targets = args.only.split(",") if args.only else list(TARGETS)
    unknown = [t for t in targets if t not in TARGETS]
    if unknown:
        sys.exit(f"unknown targets: {', '.join(unknown)} (known: {', '.join(TARGETS)})")

    packages = collect(version, targets, args.dirs)
    os.makedirs(args.out, exist_ok=True)
    sums = os.path.join(args.out, "SHA256SUMS")
    with open(sums, "w", encoding="utf-8", newline="\n") as f:
        for path, digest in packages:
            f.write(f"{digest}  {os.path.basename(path)}\n")
    notes_path = os.path.join(args.out, "notes.md")
    with open(notes_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(notes(args.tag, args.repo))

    assets = [p for path, _ in packages for p in (path, path + ".sha256")] + [sums]
    command = ["gh", "release", "create", args.tag, "--repo", args.repo, "--verify-tag",
               "--title", f"Montezuma's {version}", "--notes-file", notes_path, *assets]
    if args.dry_run:
        print("Would run:", " ".join(command))
        print(f"Notes: {notes_path}")
        return
    result = subprocess.run(command, timeout=1800)
    if result.returncode != 0:
        sys.exit(f"gh release create failed ({result.returncode})")


if __name__ == "__main__":
    main(sys.argv[1:])
