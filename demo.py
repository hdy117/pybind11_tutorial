from pathlib import Path
import sys


# Allow running demo.py directly from the repository root after building with CMake.
repo_root = Path(__file__).resolve().parent
build_dir = repo_root / "build"
if build_dir.exists():
    sys.path.insert(0, str(build_dir))

import trampoline_example as te


class Dog(te.Animal):
    def sound(self):
        return "woof from Python override"


class Cat(te.Animal):
    def sound(self):
        return "meow from Python override"


class Bird(te.AnimalPure):
    def sound(self):
        return "tweet from Python pure override"


class SilentAnimal(te.Animal):
    pass


def main():
    base = te.Animal()
    silent = SilentAnimal()
    dog = Dog()
    cat = Cat()
    bird = Bird()

    print("=== Default implementation + fallback ===")
    print("Animal.sound():      ", base.sound())
    print("SilentAnimal.sound():", silent.sound())
    print(te.describe(base))
    print(te.describe(silent))

    print("\n=== Python override through trampoline ===")
    print("Dog.sound():", dog.sound())
    print("Cat.sound():", cat.sound())
    print(te.describe(dog))
    print(te.describe(cat))
    print("Dog x3:", te.repeat_sound(dog, 3))

    print("\n=== Pure virtual trampoline example ===")
    print("Bird.sound():", bird.sound())
    print(te.describe_pure(bird))
    print("Bird x2:", te.repeat_pure_sound(bird, 2))

    print("\nNote: AnimalPure uses PYBIND11_OVERRIDE_PURE, so Python subclasses must implement sound().")


if __name__ == "__main__":
    main()
