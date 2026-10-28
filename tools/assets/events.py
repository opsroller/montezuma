# SPDX-License-Identifier: GPL-2.0
"""The plant pipeline's own events: one topic per thing that actually happens.

Not `task.succeeded`. A task succeeding is Celery's business and says nothing about
plants - every stage of this pipeline succeeds the same way, so a subscriber to it
learns only that *something* finished and has to go digging to find out what. These
topics name the thing itself, so a monitor subscribes to the one event it cares about
and knows what it has without asking anyone:

    plant.source.changed       one species' source was edited and saved        <- a stage
    plant.review.wanted        a species' frames are all in and want looking at
    plant.params.written       themes/plants.json is current with the species
    plant.atlases.drawn         the 36 leaf and bark atlases are at 4096
    plant.mesh.built            one species' meshes and materials exist      <- a stage
    plant.fault.mesh           a mesh has degenerate faces, flipped winding,
                                inverted normals, UVs across an atlas tile, or no
                                catalogue entry
    plant.pages.written         the look-dev scenes for a species exist
    plant.assets.imported       Godot's import pass finished
    plant.fault.import       an asset carries valid=false and will be skipped,
                                silently, by every later pass
    plant.frame.captured        one page is on disk at 3840x2160               <- a stage
    plant.frame.missing         a page rendered no frame
    plant.sheets.ready          the contact sheets are current                <- a stage
    plant.gpu.waiting           this run is queued behind another worktree's Godot
    plant.gpu.held              it has the GPU, after waiting that long
    plant.run.started           a pipeline run began
    plant.run.finished          it ended, with its code
    plant.run.died              it stopped without ending (reported by the monitor
                                watching its process, never by the run itself)

The three marked `<- a stage` are the ones with something after them. A monitor
subscribed to `plant.mesh.built` is what sends that species' next stage out to the
queue; a monitor on `plant.frame.captured` is what asks for the frame to be reviewed.
That is the chain: a stage finishing is an event, a monitor hears the event, and the
next stage is submitted. No stage knows what follows it, and nothing polls to find
out whether it is time.

Events go to two places, and both matter:

- **The broker**, as a topic exchange, so each monitor binds the one routing key it
  wants and redis decides who hears what. This is the live path.
- **A file** of JSON lines in `.scratch/`, appended and flushed, so the same run is
  readable afterwards, and so the pipeline still reports itself with no broker
  running at all - which is how it is usually run by hand.
"""

import json
import os
import time
from pathlib import Path

EXCHANGE = "plants"

# Every topic this pipeline publishes. A typo in a routing key is a subscriber that
# hears nothing forever, so publishing goes through `publish`, which checks the name
# against this and refuses an unknown one rather than sending it into the void.
TOPICS = {
    "plant.source.changed",
    "plant.review.wanted",
    "plant.params.written",
    "plant.atlases.drawn",
    "plant.mesh.built",
    "plant.fault.mesh",
    "plant.pages.written",
    "plant.assets.imported",
    "plant.fault.import",
    "plant.frame.captured",
    "plant.frame.missing",
    "plant.sheets.ready",
    "plant.gpu.waiting",
    "plant.gpu.held",
    "plant.run.started",
    "plant.run.finished",
    "plant.run.died",
}


class Unknown(KeyError):
    """A topic not in TOPICS. Add it there, with a line saying what it means."""


class Stream:
    """Where a run's events go: the file always, the broker when there is one.

    The broker connection is made on the first publish and never retried: a pipeline
    run by hand has no broker, and that is not a failure worth a word on every event.
    What it publishes still reaches the file, and the file is what the look-dev
    monitor follows.
    """

    def __init__(self, path, exchange=EXCHANGE, broker=None):
        self.seq = 0
        self.pid = os.getpid()
        self.began = time.monotonic()
        self.handle = None
        self.producer = None
        self.exchange = exchange
        self.broker = broker
        self.tried = False
        if path:
            Path(path).parent.mkdir(parents=True, exist_ok=True)
            self.handle = Path(path).open("a", buffering=1)

    def _producer(self):
        if self.tried or not self.broker:
            return self.producer
        self.tried = True
        try:
            from kombu import Connection, Exchange, Producer
            connection = Connection(self.broker, connect_timeout=2)
            connection.connect()
            topic = Exchange(self.exchange, type="topic", durable=False)
            self.producer = Producer(connection, exchange=topic, auto_declare=True)
        except Exception:                                 # noqa: BLE001
            self.producer = None                          # no broker; the file stands
        return self.producer

    def publish(self, topic, **fields):
        if topic not in TOPICS:
            raise Unknown(f"{topic} is not a plant pipeline topic; add it to "
                          f"tools/assets/events.py TOPICS with what it means")
        self.seq += 1
        event = {"seq": self.seq, "t": round(time.monotonic() - self.began, 2),
                 "topic": topic, "pid": self.pid, **fields}
        if self.handle:
            self.handle.write(json.dumps(event) + "\n")
            self.handle.flush()
        producer = self._producer()
        if producer is not None:
            try:
                producer.publish(event, routing_key=topic, retry=False)
            except Exception:                             # noqa: BLE001
                self.producer = None                      # the broker went away
        return event
