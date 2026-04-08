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

import minimal_binding_example as ex


print("=== Chapter 12: minimal function binding ===")
print("add(3, 4) =", ex.add(3, 4))
print(ex.greet("pybind11 learner"))
