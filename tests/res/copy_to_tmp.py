#!/usr/bin/env python3

import os
import shutil
import sys

input_path = os.path.join(
    os.getenv("MESON_SOURCE_ROOT"), os.getenv("MESON_SUBDIR"), sys.argv[1]
)

output_path = os.path.join("/tmp", sys.argv[2])

shutil.copyfile(input_path, output_path)
