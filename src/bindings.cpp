#include <pybind11/pybind11.h>

#include "animal.h"
#include "describe.h"
#include "py_animal_trampoline.h"

namespace py = pybind11;

PYBIND11_MODULE(trampoline_example, m) {
    m.doc() = "Multi-file pybind11 trampoline example";

    py::class_<Animal, PyAnimal>(m, "Animal")
        .def(py::init<>())
        .def("sound", &Animal::sound,
             "Call the virtual sound() method. Python subclasses can override it.");

    m.def("describe", &describe,
          "Call a C++ function that dispatches through Animal::sound().");
    m.def("repeat_sound", &repeat_sound,
          py::arg("animal"), py::arg("times"),
          "Call Animal::sound() repeatedly from C++.");
}
