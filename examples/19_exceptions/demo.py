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

import exception_example as ex


print("=== Chapter 19: exceptions ===")
print("checked_divide(12, 3) =", ex.checked_divide(12, 3))

try:
    ex.checked_divide(12, 0)
except RuntimeError as err:
    print("caught Python RuntimeError:", err)
