#pragma once

#include <string>
#include <vector>

struct Pet {
    std::string name;
};

class PetStore {
public:
    PetStore();

    Pet make_copy() const;
    const Pet &borrow_first() const;
    std::string describe_first() const;

private:
    std::vector<Pet> pets_;
};
