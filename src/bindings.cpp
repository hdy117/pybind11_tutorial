#include <pybind11/pybind11.h>

#include "animal.h"
#include "animal_pure.h"
#include "describe.h"
#include "describe_pure.h"
#include "py_animal_pure_trampoline.h"
#include "py_animal_trampoline.h"

namespace py = pybind11;

PYBIND11_MODULE(trampoline_example, m) {
    m.doc() = "Multi-file pybind11 trampoline examples";

    py::class_<Animal, PyAnimal>(m, "Animal")
        .def(py::init<>())
        .def("sound", &Animal::sound,
             "Call the virtual sound() method. Python subclasses can override it.");

    py::class_<AnimalPure, PyAnimalPure>(m, "AnimalPure")
        .def(py::init<>())
        .def("sound", &AnimalPure::sound,
             "Pure virtual sound(). Python subclasses must override it.");

    m.def("describe", &describe,
          "Call a C++ function that dispatches through Animal::sound().");
    m.def("repeat_sound", &repeat_sound,
          py::arg("animal"), py::arg("times"),
          "Call Animal::sound() repeatedly from C++.");

    m.def("describe_pure", &describe_pure,
          "Call a C++ function that dispatches through AnimalPure::sound().");
    m.def("repeat_pure_sound", &repeat_pure_sound,
          py::arg("animal"), py::arg("times"),
          "Call AnimalPure::sound() repeatedly from C++.");
}
