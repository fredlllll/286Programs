import os
import sys

sys.path.insert(0, os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", "stdlib")))
from buildlib import build_program

build_program({
    "source_dir": "source",
    "bootloader": os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", "bootloader.c")),
    "output": "output.img",
    "stdlib_dir": os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", "stdlib")),
})
