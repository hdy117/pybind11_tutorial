#include <pybind11/functional.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;

double heavy_compute(int n) {
    double s = 0.0;
    for (int i = 0; i < n; ++i) {
        s += i * 0.1;
    }
    return s;
}

void run_and_callback(const py::function &cb, int n) {
    double result;
    {
        py::gil_scoped_release release;
        result = heavy_compute(n);
    }
    cb(result);
}

PYBIND11_MODULE(gil_example, m) {
    m.doc() = "Chapter 21 GIL management examples";

    m.def("heavy_compute", &heavy_compute,
          py::call_guard<py::gil_scoped_release>(),
          py::arg("n"),
          "Release the GIL around pure C++ work.");

    m.def("run_and_callback", &run_and_callback,
          py::arg("cb"), py::arg("n"),
          "Release the GIL during compute, then reacquire it before calling Python.");
}
