#pragma once

#include <pybind11/pybind11.h>

#include <string>

#include "animal.h"

namespace py = pybind11;

// PyAnimal is the trampoline class.
//
// Why it exists:
// - Animal is a normal C++ base class with a virtual sound()
// - Python subclasses may override sound()
// - C++ virtual dispatch cannot see Python methods by itself
// - This intermediate C++ subclass bridges the two worlds
class PyAnimal : public Animal {
public:
    // Reuse Animal's constructors so pybind11 can instantiate the bound type.
    using Animal::Animal;

    std::string sound() const override {
        // PYBIND11_OVERRIDE does the bridge work:
        // 1. find the associated Python object
        // 2. check whether Python overrode sound()
        // 3. call the Python override if present
        // 4. convert the result back to std::string
        // 5. otherwise fall back to Animal::sound()
        PYBIND11_OVERRIDE(
            std::string,
            Animal,
            sound
        );
    }
};
