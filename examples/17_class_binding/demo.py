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

import class_binding_example as ex


point = ex.Point(1.5, 2.0)
print("=== Chapter 17: class binding ===")
print(point.describe())
point.translate(3.0, -0.5)
print(point.describe())
print("x =", point.x, "y =", point.y)
