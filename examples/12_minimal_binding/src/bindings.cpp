#include <pybind11/pybind11.h>

namespace py = pybind11;

int add(int a, int b) {
    return a + b;
}

std::string greet(const std::string &name) {
    return "Hello, " + name + " from C++";
}

PYBIND11_MODULE(minimal_binding_example, m) {
    m.doc() = "Chapter 12 minimal pybind11 function bindings";

    m.def("add", &add, py::arg("a"), py::arg("b"), "Add two integers in C++.");
    m.def("greet", &greet, py::arg("name"), "Return a greeting built in C++.");
}
