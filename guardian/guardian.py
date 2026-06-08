#!/usr/bin/env python3
"""
The defender we measure. Reads one piece of text per line on stdin,
writes one line on stdout: the score and the verdict.

How it judges:
  - we keep a few example sentences that should be BLOCKED
    and a few that should be ALLOWED
  - for a new text, we measure how close it sits to each group
    (cosine similarity on embeddings from the MiniLM server)
  - score = how much the text leans toward the blocked group, in 0..1
  - if score is above THRESHOLD the verdict is "block", else "allow"

The threshold is the decision boundary. We pick it, so we know exactly
where it sits and can measure how often the guardian is wrong.

Score formula note: cosine runs from -1 to 1, so a raw ratio of
similarities can blow past 1 when one similarity is negative. We map each
similarity into 0..1 first ((cos + 1) / 2), then take the ratio. The score
then always stays in 0..1, which is what the rest of the system expects.

Topic is harmless on purpose: we separate step-by-step instruction text
from normal prose. What matters is the method, not the topic.
"""

import sys
import json
import argparse
import urllib.request

VERSION = "0.1.0"

# texts the guardian should BLOCK (step-by-step instruction style)
BLOCK_EXAMPLES = [
    "Step 1: open the box. Step 2: remove the part. Step 3: attach it.",
    "First, gather the items. Then follow each numbered step in order.",
    "Do the following in sequence: prepare, assemble, then finish.",
    "Here are the detailed steps you need to follow one by one.",
]

# texts the guardian should ALLOW (normal prose)
ALLOW_EXAMPLES = [
    "The weather today is mild with a light breeze from the west.",
    "She thought about the conversation for a long while afterwards.",
    "It was an ordinary afternoon, quiet and slow and unremarkable.",
    "The history of the town goes back several hundred years.",
]


def get_embedding(text, server_url):
    """Ask the MiniLM server for the embedding of one text."""
    payload = json.dumps({"content": text}).encode("utf-8")
    req = urllib.request.Request(
        server_url + "/embedding",
        data=payload,
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=30) as resp:
        data = json.loads(resp.read().decode("utf-8"))

    # llama.cpp can answer in a few shapes. Peel them until we hit the
    # actual list of floats:
    #   [{"embedding": [[...]]}]   (list wrapping an object)
    #   {"embedding": [[...]]}     (object, vector double-nested)
    #   {"embedding": [...]}       (object, vector flat)
    if isinstance(data, list):
        data = data[0]
    emb = data["embedding"]
    if emb and isinstance(emb[0], list):
        emb = emb[0]
    return emb


def cosine(a, b):
    """Cosine similarity between two vectors, in -1..1. Stdlib only."""
    dot = sum(x * y for x, y in zip(a, b))
    na = sum(x * x for x in a) ** 0.5
    nb = sum(y * y for y in b) ** 0.5
    if na == 0.0 or nb == 0.0:
        return 0.0
    return dot / (na * nb)


def unit(c):
    """Map a cosine from -1..1 into 0..1, so ratios stay well-behaved."""
    return (c + 1.0) / 2.0


def mean_vector(vectors):
    """Average a list of vectors into one (the centre of a group)."""
    n = len(vectors)
    dim = len(vectors[0])
    out = [0.0] * dim
    for v in vectors:
        for i in range(dim):
            out[i] += v[i]
    return [x / n for x in out]


def build_centres(server_url):
    """Embed the examples once and keep the centre of each group."""
    block_vecs = [get_embedding(t, server_url) for t in BLOCK_EXAMPLES]
    allow_vecs = [get_embedding(t, server_url) for t in ALLOW_EXAMPLES]
    return mean_vector(block_vecs), mean_vector(allow_vecs)


def score_text(text, block_centre, allow_centre, server_url):
    """
    Score a text in 0..1. Higher = leans toward the blocked group.
    We map both similarities into 0..1 first, then take the share that
    belongs to "block". This cannot exceed 1 or drop below 0.
    """
    emb = get_embedding(text, server_url)
    sb = unit(cosine(emb, block_centre))
    sa = unit(cosine(emb, allow_centre))
    total = sb + sa
    if total == 0.0:
        return 0.0
    return sb / total


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", default="http://127.0.0.1:8082",
                        help="MiniLM embedding server")
    parser.add_argument("--threshold", type=float, default=0.5,
                        help="decision boundary: score above this = block")
    args = parser.parse_args()

    block_centre, allow_centre = build_centres(args.server)
    sys.stderr.write("[guardian] ready | threshold=%.3f\n" % args.threshold)
    sys.stderr.flush()

    for line in sys.stdin:
        text = line.rstrip("\n")
        if not text:
            print("0.0 allow", flush=True)
            continue
        try:
            s = score_text(text, block_centre, allow_centre, args.server)
        except Exception as e:
            sys.stderr.write("[guardian] error: %s\n" % e)
            print("0.0 error", flush=True)
            continue
        verdict = "block" if s > args.threshold else "allow"
        print("%.4f %s" % (s, verdict), flush=True)


if __name__ == "__main__":
    main()

