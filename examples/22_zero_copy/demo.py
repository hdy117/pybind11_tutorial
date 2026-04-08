from pathlib import Path
import sys

import numpy as np


example_dir = Path(__file__).resolve().parent
repo_root = example_dir.parents[1]
for build_dir in (
    example_dir / "build",
    repo_root / "build" / "examples" / example_dir.name,
):
    if build_dir.exists():
        sys.path.insert(0, str(build_dir))

import zero_copy_example as ex


print("=== Chapter 22: zero-copy ===")
a = np.arange(5, dtype=np.float64)
print("sum_1d(a) =", ex.sum_1d(a))
ex.scale_inplace(a, 10.0)
print("scaled a =", a)
print("make_array() =", ex.make_array())
