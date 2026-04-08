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

import lifetime_example as ex


store = ex.PetStore()
copy_pet = store.make_copy()
borrowed_pet = store.borrow_first()

print("=== Chapter 18: lifetime ===")
print(store.describe_first())
print("copy_pet.name =", copy_pet.name)
print("borrowed_pet.name =", borrowed_pet.name)
print("borrow_first uses reference_internal, so Python keeps the store alive while the borrowed pet exists.")
