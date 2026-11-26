#!/usr/bin/env python3
import re

path = "t8.shakespeare.txt"
text = open(path, "r").read().strip()
tokens = re.split(r"[\n !\"#$%&'()*+,-./:;<=>?@\[\\\]^_`{|}~]+", text)
freqs = {}

for token in tokens:
    if token in freqs:
        freqs[token] += 1
    else:
        freqs[token] = 1

print(freqs["thy"])
