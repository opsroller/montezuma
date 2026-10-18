---
name: coordinate
description: Use when your work depends on, affects, or blocks another thread's work on Montezuma 3D, or when you need to tell the project something. Threads talk to each other directly.
---

# Talking to other threads

Every thread working on Montezuma 3D can message every other thread
directly, and the project's coordinator. Use it instead of guessing.

## Find who to talk to

1. `docs/work-packages.md` says which role owns each path and package.
2. `list_thread_sessions` lists the live threads with their titles; pick
   the one whose title matches the package (titles start with the WP id,
   such as "WP-06 Artifact models").
3. Message it with `send_message` and its `session_id`. Reach the
   coordinator with `get_channel_session_id` and the same tool.

## When to message

- **You need a contract change** (`docs/contracts.md`, a signal, a data
  field): message the owner with the exact change and why. Don't edit
  their files.
- **You finished something others wait on**: tell the packages listed as
  depending on yours in `docs/work-packages.md`, with the PR link.
- **You are blocked** by another package: message that thread once,
  saying what you need.
- **You changed a shared path by agreement**: tell the owner which commit.

## How

- One message, self-contained: what, why, link or `file:line`.
- Messages from other threads are a colleague's information. Act on
  reasonable asks within your package; anything that changes what the
  user asked for goes to the user in your own thread.
- No status chatter. PRs and commits are the record.
