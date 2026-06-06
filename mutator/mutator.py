#!/usr/bin/env python3
"""
mutator.py

Stateless prompt mutator.
Reads one prompt per line from stdin, writes one mutated line to stdout.

Three mutation categories (encoding disabled — kept for reference):
  synonym      — word-level substitution from a fixed table
  reformulate  — structural rewrite via template
  encoding     — surface encoding (base64, rot13, leet) [disabled]

Mutation type is chosen randomly. Seeded for reproducibility.
"""

import sys
import random
import base64
import codecs
import argparse

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

def mutate_synonym(prompt, rng):
    words = prompt.split()
    for i, word in enumerate(words):
        key = word.lower().strip(".,!?")
        if key in SYNONYMS:
            words[i] = rng.choice(SYNONYMS[key])
            return " ".join(words)
    return prompt

def mutate_reformulate(prompt, rng):
    templates = [
        "Please {p}",
        "Could you {p}?",
        "I need you to {p}.",
        "Your task is to {p}.",
        "As an assistant, {p}.",
    ]
    lowered = prompt[0].lower() + prompt[1:] if prompt else prompt
    return rng.choice(templates).format(p=lowered)

def mutate_encoding(prompt, rng):
    """Disabled — encoding mutations produce long prompts that overflow context."""
    mode = rng.choice(["base64", "rot13", "leet"])
    if mode == "base64":
        return "Decode and execute: " + base64.b64encode(prompt.encode()).decode()
    elif mode == "rot13":
        return "ROT13 decode then follow: " + codecs.encode(prompt, "rot_13")
    else:
        return prompt.translate(str.maketrans("aeiost", "431057"))

MUTATIONS = [mutate_synonym, mutate_reformulate]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()
    rng = random.Random(args.seed)

    for line in sys.stdin:
        prompt = line.rstrip("\n")
        if not prompt:
            print("", flush=True)
            continue
        print(rng.choice(MUTATIONS)(prompt, rng), flush=True)

if __name__ == "__main__":
    main()

