#include <pybind11/pybind11.h>

#include "point.h"

namespace py = pybind11;

PYBIND11_MODULE(class_binding_example, m) {
    m.doc() = "Chapter 17 basic py::class_ binding";

    py::class_<Point>(m, "Point")
        .def(py::init<double, double>(), py::arg("x"), py::arg("y"))
        .def_property_readonly("x", &Point::x)
        .def_property_readonly("y", &Point::y)
        .def("translate", &Point::translate, py::arg("dx"), py::arg("dy"))
        .def("describe", &Point::describe);
}
