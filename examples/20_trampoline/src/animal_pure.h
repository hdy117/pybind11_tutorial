#pragma once

#include <string>

class AnimalPure {
public:
    AnimalPure() = default;
    virtual ~AnimalPure() = default;

    virtual std::string sound() const = 0;
};
