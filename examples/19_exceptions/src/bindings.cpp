#include <pybind11/pybind11.h>

#include <stdexcept>
#include <string>

namespace py = pybind11;

int checked_divide(int a, int b) {
    if (b == 0) {
        throw std::runtime_error("division by zero from C++");
    }
    return a / b;
}

PYBIND11_MODULE(exception_example, m) {
    m.doc() = "Chapter 19 exception translation example";

    m.def("checked_divide", &checked_divide, py::arg("a"), py::arg("b"),
          "Raise a C++ exception when dividing by zero.");
}
