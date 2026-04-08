#include "describe.h"

#include <sstream>
#include <stdexcept>

std::string describe(const Animal &animal) {
    return "C++ describe() heard: " + animal.sound();
}

std::string repeat_sound(const Animal &animal, int times) {
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
