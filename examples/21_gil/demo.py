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

import gil_example as ex


print("=== Chapter 21: GIL ===")
print("heavy_compute(10) =", ex.heavy_compute(10))


def report(value):
    print("Python callback received:", value)


ex.run_and_callback(report, 10)
