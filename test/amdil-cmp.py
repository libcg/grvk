#!/usr/bin/python

import hashlib
import os
import subprocess
import sys

name = sys.argv[1]
dirPath = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'res')
binPath = os.path.join(dirPath, 'il_{}.bin'.format(name))
outPath = 'il_{}_out.txt'.format(name)
refPath = os.path.join(dirPath, 'il_{}.txt'.format(name))

subprocess.run(['wine', 'test/amdil-dis.exe', binPath, outPath])

with open(outPath, 'rb') as f:
    bytes = f.read()
    outHash = hashlib.sha256(bytes).hexdigest()

with open(refPath, 'rb') as f:
    bytes = f.read()
    refHash = hashlib.sha256(bytes).hexdigest()

if outHash != refHash:
    print("got {}, expected {}".format(outHash, refHash))
    exit(1)

os.remove(outPath)
