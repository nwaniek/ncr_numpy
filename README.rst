ncr_numpy - C++20 library for numpy npz and npy files
=====================================================

tl;dr: go to `example.cpp <examples/example.cpp>`_

What
----
ncr_numpy is a C++ library with minimal dependencies for reading and writing npz
and npy files, and uses features of C++20. The library also provides a simple
n-dimensional array implementation that is kept independent of the numpy file
I/O.

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


Reason
------
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


Goals
-----
The goals of ncr_numpy is to achieve a robust, correct, and fast C++ library to
load and write numpy data from regular and compressed files, while supporting
not only basic types (numpy's built in types) but arbitrary structured arrays
such as nested structs with mixed endianness. A further goal is to establish an
API interface which is easy to use, while also providing a functions that allow
to improve performance and reduce the number if intermediary calls.

Finally, ncr_numpy is supposed to integrate nicely within ncr, while being a
standalone library. Note that both, ncr and ncr_numpy, share some files (e.g.
ncr_types.hpp and ncr_bits.hpp).


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
        year = {2023}
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

**Q**: Why does ncr_numpy not provide CMake build files to create a library?
**A**: CMake is an abomination which should have never seen the light of day.
If you wish or need to use it, it should be no problem for you to write your own
build files. Everyone else should use a sane alternative, like a simple and
plain Makefile (see examples/Makefile).

**Q**: Why is there a difference between the files generated by numpy and ncr_numpy?
**A**: numpy commonly writes files using numpy libformat file version 1.0, while
ncr_numpy writes files for libformat file version 2.0. The main difference
between the files is that files of version 1.0 use 2 bytes to store the size of
the header information, while version 2.0 uses 4 bytes. The description string
and the payload remain the same (up to a certain file size limit). This can be
verified by looking at a hex-dump of the files. For an example how to generate
such a hexdump, see examples/example.cpp.

**Q**: is `ncr_numpy` without any errors and does it support everything that
`numpy` arrays provide?
**A**: No, and no. First, it is highly unlikely that any software project has no
errors. Still, the goal is to reduce errors as much es possible and continuously
improve `ncr_numpy`. Second, `ncr_numpy` is not a full implementation of
`numpy`'s ndarray, but rather for loading `numpy` arrays from `.npy` and `.npz`
files. Hence, the `ndarray` that is provided with `ncr_numpy` only provides a
very small subset of functions to work with n-dimensional arrays in C++. If you
need more functionality, in particular for mathematical operations, please have
a look at mature C++ math libraries such as `Eigen`, `blaze`, or `Armadillo`.


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
