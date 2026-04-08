#define PY_SSIZE_T_CLEAN
#include <Python.h>

static PyObject *raw_add(PyObject *self, PyObject *args) {
    int a;
    int b;
    if (!PyArg_ParseTuple(args, "ii", &a, &b)) {
        return NULL;
    }
    return PyLong_FromLong(a + b);
}

static PyMethodDef module_methods[] = {
    {"add", raw_add, METH_VARARGS, "Add two integers using the raw CPython C API."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef module_def = {
    PyModuleDef_HEAD_INIT,
    "raw_binding_example",
    "Chapter 23 handwritten CPython extension example.",
    -1,
    module_methods
};

PyMODINIT_FUNC PyInit_raw_binding_example(void) {
    return PyModule_Create(&module_def);
}
