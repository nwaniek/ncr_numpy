ncr_numpy - C++20 library for numpy npz and npy files
=====================================================

tl;dr: go to `example.cpp <examples/example.cpp>`_


What
----
ncr_numpy is a C++ library with minimal dependencies for reading and writing npz
and npy files, and uses features of C++20. The library also provides a simple
n-dimensional array implementation that is kept independent of the numpy file
I/O.


Goals
-----
The main goal of ncr_numpy is to achieve a robust, correct, and fast C++ library
to load and write numpy data from regular and compressed files, while supporting
not only basic types (numpy's built in types) but arbitrary structured arrays
such as nested structs with mixed endianness. Another goal is to establish an
API interface which is easy to use, while also providing functions that allow to
improve performance and reduce the number of intermediary calls when necessary.

Finally, ncr_numpy is supposed to integrate nicely within ncr, while being a
standalone library. Note that both, ncr and ncr_numpy, share some files (e.g.
ncr_types.hpp and ncr_bits.hpp).


Features
--------
* read and write numpy npy files
* read and write numpy npz files (zip archives)
* support structured arrays of arbitrary complexity
* support data with mixed endianness
* provide a simple ndarray implementation for arbitrary tensors and data
  structures
* provide a facade around ndarray for basic types
* uses C++20 ranges to achieve a clean API interface
* provide a simple solution to swap out the zip backend for ease of
  customization


Installation and Usage
----------------------
To use `ncr_numpy`, make sure that your compiler can find the header files.
Thus, either add the path to `ncr_numpy` to your list of includes or copy the
header files to your preferred location. Also make sure to compile a zip backend
implementation if you want to use npz files. Currently, `ncr_numpy` ships with
an implementation that is based on `libzip <libzip>`_ in file
`ncr/core/impl/zip_libzip.hpp <include/ncr/core/impl/zip_libzip.hpp>`_. A simple
`Makefile <examples/Makefile>`_ as well as a basic `CMakeLists.txt <examples/CMakeLists.txt>`_
can be found in the `examples <examples>`_ folder.

Using `ncr_numpy` to load data from a file is as simple as:

.. code-block:: c++

    auto val = ncr::numpy::load(your_filepath);
    if (std::holds_alternative<ncr::numpy::ndarray>(val)) {
        auto arr = std::get<ncr::numpy::ndarray>(val);
        // do something with the array
    }

Alternatively, you can use a slightly more verbose interface if the file type is
known beforehand:

.. code-block:: c++

    ncr::numpy::npzfile npz;
    ncr::numpy::from_npz("some/file.npz", npz);
    std::cout << "shape = ";
    ncr::numpy::serialize_shape(std::cout, npz["array_name"].shape);
    std::cout << "\n";

Data that is read from a simple numpy file is written to an `ndarray`.
In addition, there exist `ndarray_t`, which is simply a template based facade
for `ndarray` to make working with known basic data types easier.
`ndarray` and `ndarray_t` support common ways to access data:

.. code-block:: c++

    ncr::numpy::ndarray_t<f64> arr;
    ncr::numpy::from_npy("assets/in/simpletensor1.npy", arr);
    arr(0, 0, 0) = 7.0;
    arr(1, 1, 1) = 17.0;
    arr(1, 2, 3) = 23.1234;
    print_tensor(arr, "  ");

Note that `ndarray` and `ndarray_t` currently do not support any math
operations. While this might change in the future, the recommended approach is
to use existing libraries such as `Eigen` or `Armadillo`.

It is also possible to read from .npz files into an `npzfile` instance, as long
as a zip backend is compiled (see comment above). `npzfile` is merely a thin
wrapper around an `std::vector` and two `std::map` instances that store the
names of the arrays and the arrays themselves.

.. code-block:: c++

    ncr::numpy::from_npz("assets/in/multiple_named.npz", npz);
    for (auto const& name: npz.names) {
        auto shape = npz[name].shape();
        std::cout << name << ".shape = ";
        ncr::numpy::serialize_shape(std::cout, shape);
        std::cout << "\n";
    }

This example uses `ncr::numpy::serialize_shape`, which is a utility function to
turn the shape of an ndarray into something readable.

Working with (known) structured arrays is straightforward. The following example
assumes that student data, with a name and two grades per student, are stored in
a npy file.

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


See `example.cpp <examples/example.cpp>`_ for further and longer examples on how
to use `ncr_numpy`.


Design Principles
-----------------
For ease of use, the library attempts to replicate the API interface of numpy's
load and save functions. At the same time, a slightly advanced but more verbose
API allows to get the most out of ncr_numpy. Moreover, the ndarray
implementation by default returns an std::ranges subrange to a vector of
uint8_t, which makes adapting the array to complex data types and structs as
easy as possible. A facade template `ndarray_t` makes working with ndarray that
contain basic types straightforward (see examples.cpp:example_facade()).

For ease of customization, the library is written in a way which makes swapping
out parts or adapting it to complex data easy. For instance, the library
currently uses libzip to read and write npz files (which are in fact simple zip
archives of npy files), but this particular backend to work with zip archives
can be replaced by simply implementing a few required functions and compiling
against the new implementation.

To achieve the goal of supporting arbitrary structured arrays, ncr_numpy
includes a basic recursive descent parser (RDP) and a backtracking tokenizer.
Both the parser and the tokenizer can be used independently of ncr_numpy. Note,
however, that they do not support the full python formal grammar, but only the
subset required for ncr_numpy.


Reason, or why another C++ numpy loader?
----------------------------------------
Existing implementations do not provide the functionality I need or are not as
robust as I would like. For instance, they are not necessarily able to handle
structured arrays of arbitrary depth, or data with mixed endianness. Some
solutions assume that certain fields in the numpy description header must exist,
which is wrong. Others throw exceptions in the library code (i.e.  the code
which loads the files), which I personally dislike. That is, while exceptions
can be a good tool, I prefer to have return codes in functions that should be
considered *library code*. Then, simply testing if the file size corresponds to
the item-size is rarely checked. Anyway, the list goes on and at some point I
decided to simply roll my own.


Usage Guidelines
----------------
There are no explicit rules when using `ncr_numpy` except following the MIT
License (see below, or the `LICENSE <LICENSE>`_ file). Still, if you use
`ncr_numpy` or other parts of the `ncr` ecosystem in your work, it would be
great if you could credit them either by explicitly referencing this website or
`https://rochus.net`, or even better, cite one of my papers.

If none of my existing papers fits your bill (likely), then you could use for
instance the following (bibtex) snippet:

.. code::

    @Misc{ncr,
        author =   {Nicolai Waniek},
        title =    {{ncr_numpy}: {A C++20 interface for numpy files}},
        howpublished = {\url{https://github.com/nwaniek/ncr_numpy}}
        year = {2023--2024}
    }

There might be a proper paper to cite in the future. Stay tuned.

If you want to donate to this project, get in touch with me. Alternatively, tell
your favorite funding agency or company to (continue to) fund my research.


Contributing
------------
If you wish to contribute to this project, please open pull requests, post
clearly written features requests or bug reports. Regarding feature requests,
please be aware that feature requests that go significantly beyond the purpose
of ncr_numpy will not be followed up.


FQA (Frequently Questioned Answers)
-----------------------------------
**Q**: I found a bug!
**A**: That's (maybe not so) great! :) please provide a full report with a
minimial working example to reproduce the bug.

**Q**: How to use and build it?
**A**: ncr_numpy provides a simple and plain Makefile for the example
application (see `examples/Makefile <examples/Makefile>`_). To build the
examples, go to the `examples/ <examples/>`_ directory, run :code:`make` to
build the application, followed by :code:`./example` to run the examples.
The Makefile can be easily adjusted to specific requirements, or the relevant
portions extracted to other build systems. The most important aspect is to point
your build system to ncr_numpy's headers, and in case you use a zip backend, to
the corresponding implementation file.
In addition to the Makefile, the examples directory contains a basic cmake
`CMakeLists.txt <examples/CMakeLists.txt>`_.  To build and run the example
application using cmake, go to `examples/ <examples/>`_, run :code:`cmake -S
. -B build && cmake --build build` followed by :code:`./build/example`.

**Q**: Why is there a difference between the files generated by numpy and ncr_numpy?
**A**: numpy commonly writes files using numpy libformat file version 1.0, while
ncr_numpy writes files for libformat file version 2.0. The main difference
between the files is that files of version 1.0 use 2 bytes to store the size of
the header information, while version 2.0 uses 4 bytes. The description string
and the payload remain the same (up to a certain file size limit). This can be
verified by looking at a hex-dump of the files. For an example how to generate
such a hexdump, see `examples/example.cpp <examples/example.cpp>`_.

**Q**: is `ncr_numpy` without any errors and does it support everything that
`numpy` arrays provide?
**A**: No, and no. First, it is highly unlikely that any software project has no
errors. Still, the goal is to reduce errors as much es possible and continuously
improve `ncr_numpy`. Second, `ncr_numpy` is not a full implementation of
`numpy`'s ndarray, but rather for loading `numpy` arrays from `.npy` and `.npz`
files. Hence, the `ndarray` that is provided with `ncr_numpy` only provides a
very small subset of functions to work with n-dimensional arrays in C++. If you
need more functionality, in particular for mathematical operations, please have
a look at mature C++ math libraries such as
`Eigen <https://eigen.tuxfamily.org>`_,
`blaze <https://github.com/dendisuhubdy/blaze>`_, or
`Armadillo <https://arma.sourceforge.net>`_.

**Q**: what are `include/ncr/core <include/ncr/core>`_ and `include/ncr/core.hpp <include/ncr/core.hpp>`_?
**A**: They belong to `ncr_core`, a separate header only library that is used in
a variety of projects. Essentially, `ncr_core` contains basic definitions,
algorithms, and data structures. It does not change much, and development of it
happens within its own repository. The files shipped with `ncr_numpy` are simply
copied from that other repository. While this contains slightly more than what
`ncr_numpy` actually uses, this makes deployment and development easier, and
also makes `ncr_numpy` fully self contained. Consequently, a user doesn't need
to checkout another submodule or copy other files around that what is already
available in `ncr_numpy`. It also allows to use several other `ncr_*` libraries,
because the files are synchronized. Finally, both `ncr_core` and `ncr_numpy` are
managed as submodules of a monorepo, within which copying and integrity testing
is automated.

Related (ncr) projects
----------------------

* `ncr <http://github.com/nwaniek/ncr>`_ the Neural and Natural Computation
  Repository. `ncr` contains C++20 headers for all kind of purposes, but with a
  focus on neural computation and (numerical) simulation of natural processes
  and dynamics.


License Information
-------------------
MIT License. See `LICENSE <LICENSE>`_ for more details.


Authors
-------
Nicolai Waniek
