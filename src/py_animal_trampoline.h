#pragma once

#include <pybind11/pybind11.h>

#include <string>

#include "animal.h"

namespace py = pybind11;

// Trampoline class: bridges C++ virtual dispatch to Python overrides.
class PyAnimal : public Animal {
public:
    using Animal::Animal;

    std::string sound() const override {
        PYBIND11_OVERRIDE(
            std::string,
            Animal,
            sound
        );
    }
};
