# SPDX-License-Identifier: GPL-2.0
"""What the plugin's monitors share: where the checkout is, and how to say something.

A library, not a program. Each monitor is its own executable on its own event source
and is declared on its own line of `monitors/monitors.json`; Claude Code runs each as
its own process. Nothing here dispatches between them.
"""

import os
import sys
import time
import subprocess
import threading
from pathlib import Path

PIPELINE = Path("tools/assets/lookdev.py")

_OUT = threading.Lock()


def project_root():
    """The checkout this monitor was started in, from git, never a written path.

    A monitor runs in the session's working directory, which is this checkout today
    and somebody else's tomorrow. Returns None when it is neither, and the caller
    exits quietly: a monitor that complains once a session in every unrelated
    project is worse than no monitor.
    """
    found = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"],
        capture_output=True, text=True, check=False,
    )
    if found.returncode == 0 and found.stdout.strip():
        root = Path(found.stdout.strip())
        if (root / PIPELINE).exists():
            return root
    project = os.environ.get("CLAUDE_PROJECT_DIR")
    if project and (Path(project) / PIPELINE).exists():
        return Path(project)
    return None


def speaker(tag):
    """A `say` for one monitor. Locked, because a monitor with a watch thread prints
    from two places and a notification split down the middle reads as two."""
    def say(text):
        with _OUT:
            print(f"{tag}: {text}", flush=True)
    return say


def clock(seconds):
    seconds = int(seconds or 0)
    return f"{seconds // 60}m{seconds % 60:02d}s" if seconds >= 60 else f"{seconds}s"


def tail(path, root):
    """Follow a file as it is written.

    `tail -F` waits on an inotify watch rather than waking up to look, retries a file
    that does not exist yet, and reopens one that is replaced - so a monitor is armed
    before anything has run and stays armed across any number of runs.

    Where it starts reading depends on whether the file is already there, and getting
    this wrong loses whole runs. A file that exists holds an earlier session's
    history, so following it from the end reports what happens next rather than
    replaying what already happened. A file that does not exist yet has no history to
    replay, and `tail` only notices it on its next retry - so a short run that starts
    and finishes inside that window would be skipped entirely if this seeked to the
    end of what by then is a complete run. From the top, nothing is missed either way.
    """
    start = "0" if Path(path).exists() else "+1"
    child = subprocess.Popen(
        ["tail", "-F", "-n", start, "--", str(path)],
        cwd=root, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
        text=True, bufsize=1,
    )
    try:
        with child.stdout:
            for line in child.stdout:
                yield line.rstrip("\n")
    finally:
        child.terminate()


def pipeline(root):
    """The pipeline's own modules, for a monitor that publishes work back into it."""
    sys.path.insert(0, str(root / "tools/assets"))
    import events                                        # noqa: PLC0415
    return events


def subscribe(root, topics, handle, say, every=5):
    """Receive the topics this monitor is for, and nothing else.

    The routing keys are bound on the broker, so a monitor is subscribed to its own
    stream rather than to everything with a filter in front of it: redis decides who
    hears what, which is what a topic exchange is for. Two monitors on two topics are
    two subscriptions, not one stream and a switch.

    A monitor may name more than one key - the six stage completions have six names
    and belong together - and they are bound as several bindings on its one queue, so
    the broker is still what routes them. A pattern (`plant.fault.*`) binds a group
    in one key where the names allow it.

    The reconnect loop is for a broker that is not up yet - monitors start with the
    session and the broker starts with `plants-queue up`, in that order - and for one
    that is restarted under us. It is a retry on a connection, not a poll for state:
    nothing here asks whether anything has happened.
    """
    from kombu import Connection, Exchange, Queue, binding  # noqa: PLC0415

    events = pipeline(root)
    names = [topics] if isinstance(topics, str) else list(topics)
    for name in names:
        # A mistyped routing key is a monitor that hears nothing for ever and looks
        # exactly like a quiet pipeline, so a plain name is checked against the
        # vocabulary. A pattern is not a name and cannot be.
        if "*" not in name and "#" not in name and name not in events.TOPICS:
            raise events.Unknown(
                f"{name} is not a plant pipeline topic; the topics are in "
                f"tools/assets/events.py"
            )
    socket = root / ".scratch/redis.sock"
    url = f"redis+socket://{socket}"
    exchange = Exchange(events.EXCHANGE, type="topic", durable=False)
    # Named after what it subscribes to and exclusive to this process, so each
    # monitor has its own queue and two of them never eat each other's events.
    queue = Queue(
        f"monitor.{'+'.join(names)}",
        bindings=[binding(exchange, routing_key=name) for name in names],
        durable=False, auto_delete=True,
    )
    topic = ", ".join(names)
    told = False
    while True:
        try:
            with Connection(url, connect_timeout=2) as connection:
                connection.connect()
                if told:
                    say(f"subscribed to {topic}")
                    told = False
                with connection.Consumer(queue, callbacks=[
                    lambda body, message: (handle(body), message.ack())
                ], accept=["json"]):
                    while True:
                        connection.drain_events(timeout=None)
        except KeyboardInterrupt:
            return 0
        except Exception:                                # noqa: BLE001
            # No broker yet, or it went away. Say it once, then keep trying quietly:
            # a monitor that reports every retry is worse than the silence it is
            # meant to replace.
            if not told:
                told = True
                say(f"waiting for the broker; run `plants-queue up` to start it "
                    f"({topic})")
            time.sleep(every)


def main(watch):
    """Every monitor's entry point: find the checkout, or exit quietly."""
    root = project_root()
    if root is None:
        return 0
    try:
        return watch(root)
    except KeyboardInterrupt:
        return 0
    except BrokenPipeError:
        # The session ended and nothing is reading. Not a failure.
        return 0
