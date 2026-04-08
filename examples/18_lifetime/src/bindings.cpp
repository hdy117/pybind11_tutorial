#include <pybind11/pybind11.h>

#include "pet_store.h"

namespace py = pybind11;

PYBIND11_MODULE(lifetime_example, m) {
    m.doc() = "Chapter 18 lifetime and return-value policy examples";

    py::class_<Pet>(m, "Pet")
        .def_readonly("name", &Pet::name);

    py::class_<PetStore>(m, "PetStore")
        .def(py::init<>())
        .def("describe_first", &PetStore::describe_first)
        .def("make_copy", &PetStore::make_copy,
             "Return a by-value copy of the first pet.")
        .def("borrow_first", &PetStore::borrow_first,
             py::return_value_policy::reference_internal,
             "Return a reference tied to the store's lifetime.");
}
