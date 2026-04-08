#include "pet_store.h"

PetStore::PetStore() : pets_{{"Mochi"}, {"Pudding"}} {}

Pet PetStore::make_copy() const {
    return pets_.front();
}

const Pet &PetStore::borrow_first() const {
    return pets_.front();
}

std::string PetStore::describe_first() const {
    return "first pet is " + pets_.front().name;
}
