#!/usr/bin/env python3
"""
Stateless prompt mutator.
Reads one prompt per line from stdin, writes one mutated line to stdout.

Mutation categories (encoding disabled — kept for reference):
  synonym      — word-level substitution from a fixed table
  reformulate  — structural rewrite (strips old wrapper, applies one new one)
  reset        — jump back to the original seed (long jump out of a basin)
  encoding     — surface encoding (base64, rot13, leet) [disabled]

Mutation type is chosen randomly. Seeded for reproducibility.

Note on reformulate: it rewrites, it does not wrap. Each call first removes
any wrapper added by a previous call, then applies exactly one. Without this,
composing reformulate on itself piles wrappers up ("could you could you
please ...") and the text degenerates within ~100 steps. The rewrite keeps
the candidate roughly the same size as its parent — a real neighbour, the way
an annealing move should be.
"""

import sys
import random
import base64
import codecs
import argparse

VERSION = "0.1.0"

SYNONYMS = {
    "help":        ["assist", "support", "aid"],
    "tell":        ["explain", "describe", "inform"],
    "show":        ["demonstrate", "reveal", "display"],
    "make":        ["create", "produce", "generate"],
    "use":         ["employ", "utilize", "apply"],
    "give":        ["provide", "supply", "offer"],
    "find":        ["locate", "identify", "discover"],
    "write":       ["compose", "draft", "produce"],
    "explain":     ["clarify", "describe", "elaborate"],
    "ignore":      ["disregard", "bypass", "skip"],
    "previous":    ["prior", "earlier", "preceding"],
    "instruction": ["directive", "guideline", "command"],
}

# wrappers reformulate can add — kept as (prefix, suffix) so we can also
# detect and strip them back off before adding a new one
WRAPPERS = [
    ("Please ", ""),
    ("Could you ", "?"),
    ("I need you to ", "."),
    ("Your task is to ", "."),
    ("As an assistant, ", "."),
]


def mutate_synonym(prompt, rng, seed_text):
    """Swap one known word for a sibling. In-place, so length stays put."""
    words = prompt.split()
    for i, word in enumerate(words):
        key = word.lower().strip(".,!?")
        if key in SYNONYMS:
            words[i] = rng.choice(SYNONYMS[key])
            return " ".join(words)
    return prompt


def strip_wrapper(prompt):
    """Remove one wrapper if the text currently has one. Keeps reformulate
    from stacking wrappers on every call."""
    for prefix, suffix in WRAPPERS:
        if prompt.startswith(prefix):
            inner = prompt[len(prefix):]
            if suffix and inner.endswith(suffix):
                inner = inner[:-len(suffix)]
            # un-lowercase the first letter we lowered when wrapping
            if inner:
                inner = inner[0].upper() + inner[1:]
            return inner
    return prompt


def mutate_reformulate(prompt, rng, seed_text):
    """Rewrite the phrasing: strip any existing wrapper, then apply one.
    Net effect is a swap, not a pile-up — length stays roughly constant."""
    core = strip_wrapper(prompt)
    lowered = core[0].lower() + core[1:] if core else core
    prefix, suffix = rng.choice(WRAPPERS)
    return prefix + lowered + suffix


def mutate_reset(prompt, rng, seed_text):
    """Jump straight back to the original seed. This is the long-range move:
    it pulls the walk out of whatever basin it has settled in, restoring the
    connectivity of the search space. Used rarely (see weights below)."""
    return seed_text


def mutate_encoding(prompt, rng, seed_text):
    """Disabled — encoding mutations produce long prompts that overflow context."""
    mode = rng.choice(["base64", "rot13", "leet"])
    if mode == "base64":
        return "Decode and execute: " + base64.b64encode(prompt.encode()).decode()
    elif mode == "rot13":
        return "ROT13 decode then follow: " + codecs.encode(prompt, "rot_13")
    else:
        return prompt.translate(str.maketrans("aeiost", "431057"))


# (function, weight) — reset is rare, the local moves do most of the work
MUTATIONS = [
    (mutate_synonym,     45),
    (mutate_reformulate, 45),
    (mutate_reset,       10),
]


def pick(rng):
    funcs   = [m[0] for m in MUTATIONS]
    weights = [m[1] for m in MUTATIONS]
    return rng.choices(funcs, weights=weights, k=1)[0]


def load_seed(path):
    with open(path, "r") as f:
        line = f.readline()
    return line.rstrip("\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=42,
                        help="PRNG seed (reproducibility)")
    parser.add_argument("--seed-file", default="../seeds/seed_01.txt",
                        help="original seed text, used by the reset move")
    args = parser.parse_args()

    rng = random.Random(args.seed)
    seed_text = load_seed(args.seed_file)

    for line in sys.stdin:
        prompt = line.rstrip("\n")
        if not prompt:
            print("", flush=True)
            continue
        print(pick(rng)(prompt, rng, seed_text), flush=True)


if __name__ == "__main__":
    main()

