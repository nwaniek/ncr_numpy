ncr_numpy: A C++ 20 header only numpy file library
===========================================================

**Authors**:
Nicolai Waniek
*Department of Mathematical Sciences, Norwegian University of Science and Technology, Trondheim, Norway

**Corresponding author**: nicolai.s.waniek@ntnu.no

**Repository**: https://github.com/nwaniek/ncr_numpy

**License**: MIT

Summary
-------

``ncr_numpy`` is a C++20 single-header library with for reading and writing
numpy .npy and .npz files, and includes a basic n-dimensional array
implementation.

The main goal of ``ncr_numpy`` is to achieve a robust, correct, and fast C++ library
to load and write numpy data from regular and compressed files, while supporting
not only basic types (numpy's built in types) but arbitrary structured arrays
such as nested structured arrays with mixed endianness. Another goal is to
establish an API interface which is easy to use, while also providing functions
that allow to improve performance and reduce the number of intermediary calls
when necessary.


Statement of Need
-----------------

The numpy file formats .npy and .npz are widely used, for instance in data
science and machine learning. The data format for both are deliberately
straightforward and several C and C++ libraries exist to read them. Yet, none of
the existing libraries can handle arbitrary nexted structured arrays, as this
requires parsing the string representation of a potentially nested Python
dictionary. ``ncr_numpy`` includes such a parser, and therefore can handle
nested structs that are comprised of basic numpy datatypes. Other libraries use
a particular library as backend for reading .npz files, for instance libzip,
without the ability to change the backend. In contrast, ``ncr_numpy`` provides a
simple interface to swap the libzip based backend it provides with a custom
backend for reading and writing zip files.


Functionality
-------------

The features of ``ncr_numpy`` include, but are not limited to

* read and write numpy npy files
* read and write numpy npz files (zip archives)
* support structured arrays of arbitrary complexity
* support data with mixed endianness
* provide a simple ndarray implementation for arbitrary tensors and data structures
* provide a facade around ndarray for basic types
* uses C++20 ranges to achieve a clean API interface
* provide a simple solution to swap out the zip backend for ease of customization


Example Usage
-------------

Using ``ncr_numpy`` to load data from a file is as simple as

.. code-block:: c++

    auto val = ncr::numpy::load(your_filepath);
    if (std::holds_alternative<ncr::numpy::ndarray>(val)) {
        auto arr = std::get<ncr::numpy::ndarray>(val);
        // do something with the array
    }

Alternatively, a verbose interface allows to work with files whose structure is
known a priori:

.. code-block:: c++

    ncr::numpy::npzfile npz;
    ncr::numpy::from_npz("some/file.npz", npz);
    std::cout << "shape = ";
    ncr::numpy::serialize_shape(std::cout, npz["array_name"].shape);
    std::cout << "\n";

Data that is read from a simple numpy file is written to an instance of
`ndarray`, which is a general but minimal n-dimensional array implementation
that works for any data type or structure contained in a numpy file.  Along
``ndarray``, ``ncr_numpy`` ships `ndarray_t`, which is a template-based facade
for `ndarray`, meaning it takes the data type that is contained within a .npy
file as template argument, to make working with known basic data types easier.
`ndarray` and `ndarray_t` support common ways to access data as presented in the
following example.

.. code-block:: c++

    ncr::numpy::ndarray_t<f64> arr;
    ncr::numpy::from_npy("assets/in/simpletensor1.npy", arr);
    arr(0, 0, 0) = 7.0;
    arr(1, 1, 1) = 17.0;
    arr(1, 2, 3) = 23.1234;
    print_tensor(arr, "  ");

Note that `ndarray` and `ndarray_t` do not support any math operations, and the
recommended approach is to use existing, stable libraries such as `Eigen` or
`Armadillo`.

``ncr_numpy`` also provides the ability to read from .npz files into an
`npzfile` instance. Note that `npzfile` is just a thin wrapper around an
`std::vector` and two `std::map` instances that store the names of the arrays
and the arrays themselves, as shown in the next example.

.. code-block:: c++

    ncr::numpy::from_npz("assets/in/multiple_named.npz", npz);
    for (auto const& name: npz.names) {
        auto shape = npz[name].shape();
        std::cout << name << ".shape = ";
        ncr::numpy::serialize_shape(std::cout, shape);
        std::cout << "\n";
    }

This example uses `ncr::numpy::serialize_shape`, which is a utility function to
turn the shape of an ndarray into human readable output.

Working with (known) structured arrays is straightforward in ``ncr_numpy``. The
following example assumes that student data, with a name and two grades per
student, are stored in a npy file.

.. code-block:: c++

    struct student_t
    {
        ucs4string<16> name;
        f64 grades[2];
    };

    ncr::numpy::ndarray arr;
    ncr::numpy::npyfile npy;
    ncr::numpy::from_npy("assets/in/structured.npy", arr, &npy);

    student_t &student = arr.value<student_t>(0);
    std::cout << student.name << " has grades " << student.grades[0] << " and " << student.grades[1] << "\n";

``ncr_numpy`` provides several more examples.


Installation
------------

``ncr_numpy`` does not need to be installed. Rather, the header file
``ncr_numpy.hpp`` needs to be placed into a folder that is on the include path
of the compiler, for instance the C++ project directory of the project under
consideration, or often an ``include`` folder underneath it . Afterwards,
``ncr_numpy`` can be used by including it in the C++ translation unit where it
is needed.

Note that ``ncr_numpy.hpp`` ships with a zip implementation that uses `libzip
<libzip>`_ as backend. To disable this backend, the ``NCR_DISABLE_ZIP_LIBZIP``
compiler flag can be passed to ``ncr_numpy.hpp`` or set before including the
header. This will disable all zip related functionality, i.e. reading .npz
files, as long as no other zip backend is provided.


Development and Extension
-------------------------

Related Work
------------

Acknowledgements
----------------

References
----------

- NumPy: https://numpy.org
