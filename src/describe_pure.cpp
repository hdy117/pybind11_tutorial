#include "describe_pure.h"

#include <sstream>
#include <stdexcept>

std::string describe_pure(const AnimalPure &animal) {
    return "C++ describe_pure() heard: " + animal.sound();
}

std::string repeat_pure_sound(const AnimalPure &animal, int times) {
    if (times < 0) {
        throw std::runtime_error("times must be non-negative");
    }

    std::ostringstream out;
    for (int i = 0; i < times; ++i) {
        if (i > 0) {
            out << " | ";
        }
        out << animal.sound();
    }
    return out.str();
}
