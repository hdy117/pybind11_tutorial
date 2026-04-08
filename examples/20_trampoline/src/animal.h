#pragma once

#include <string>

class Animal {
public:
    Animal() = default;
    virtual ~Animal() = default;

    virtual std::string sound() const {
        return "...";
    }
};
