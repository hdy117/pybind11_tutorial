from pathlib import Path
import sys


example_dir = Path(__file__).resolve().parent
repo_root = example_dir.parents[1]
for build_dir in (
    example_dir / "build",
    repo_root / "build" / "examples" / example_dir.name,
):
    if build_dir.exists():
        sys.path.insert(0, str(build_dir))

import raw_binding_example as ex


print("=== Chapter 23: reinvent minimal binding ===")
print("This example uses the raw CPython C API instead of pybind11.")
print("add(20, 22) =", ex.add(20, 22))
