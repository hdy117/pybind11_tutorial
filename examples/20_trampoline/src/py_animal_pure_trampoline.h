#pragma once

#include <pybind11/pybind11.h>

#include <string>

#include "animal_pure.h"

namespace py = pybind11;

// Pure-virtual trampoline:
// Python subclasses must override sound(), otherwise pybind11 will raise.
class PyAnimalPure : public AnimalPure {
public:
    using AnimalPure::AnimalPure;

    std::string sound() const override {
        PYBIND11_OVERRIDE_PURE(
            std::string,
            AnimalPure,
            sound
        );
    }
};
