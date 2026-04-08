#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;

double sum_1d(py::array_t<double, py::array::c_style> arr) {
    if (arr.ndim() != 1) {
        throw std::runtime_error("expected a 1D float64 array");
    }

    auto v = arr.unchecked<1>();
    double s = 0.0;
    for (py::ssize_t i = 0; i < v.shape(0); ++i) {
        s += v(i);
    }
    return s;
}

void scale_inplace(py::array_t<double, py::array::c_style> arr, double alpha) {
    if (arr.ndim() != 1) {
        throw std::runtime_error("expected a 1D float64 array");
    }

    auto v = arr.mutable_unchecked<1>();
    for (py::ssize_t i = 0; i < v.shape(0); ++i) {
        v(i) *= alpha;
    }
}

py::array_t<double> make_array() {
    constexpr py::ssize_t n = 8;
    auto *data = new double[n];
    for (py::ssize_t i = 0; i < n; ++i) {
        data[i] = static_cast<double>(i);
    }

    py::capsule owner(data, [](void *p) {
        delete[] static_cast<double *>(p);
    });

    return py::array_t<double>(
        {n},
        {static_cast<py::ssize_t>(sizeof(double))},
        data,
        owner
    );
}

PYBIND11_MODULE(zero_copy_example, m) {
    m.doc() = "Chapter 22 zero-copy, NumPy, and owner-handling examples";

    m.def("sum_1d", &sum_1d, py::arg("arr"),
          "Read a strict 1D float64 C-contiguous array without forcecast.");
    m.def("scale_inplace", &scale_inplace, py::arg("arr"), py::arg("alpha"),
          "Scale a strict 1D float64 C-contiguous array in place.");
    m.def("make_array", &make_array,
          "Return a NumPy array view backed by C++ heap memory owned by a capsule.");
}
