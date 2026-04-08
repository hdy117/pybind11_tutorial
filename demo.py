import trampoline_example as te


class Dog(te.Animal):
    def sound(self):
        return "woof from Python override"


class Cat(te.Animal):
    def sound(self):
        return "meow from Python override"


def main():
    base = te.Animal()
    dog = Dog()
    cat = Cat()

    print("=== Direct method calls ===")
    print("Animal.sound():", base.sound())
    print("Dog.sound():   ", dog.sound())
    print("Cat.sound():   ", cat.sound())

    print("\n=== C++ virtual dispatch through trampoline ===")
    print(te.describe(base))
    print(te.describe(dog))
    print(te.describe(cat))

    print("\n=== Repeated C++ dispatch ===")
    print("Dog x3:", te.repeat_sound(dog, 3))
    print("Cat x2:", te.repeat_sound(cat, 2))


if __name__ == "__main__":
    main()
