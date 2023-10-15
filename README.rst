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
load and save functions, while at the same time providing an advanced API for
sophisticated users. Moreover, the ndarray implementation by default returns an
std::ranges subrange to a vector of uint8_t, which makes adapting the array to
complex data types as straightforward as possible. A simple interface is
provided for basic dtypes.

For ease of customization, the library is written in a way which makes swapping
out parts easy or adapting it to complex data. For instance, the library
currently uses libzip to read and write npz files (which are in fact simple zip
archives of npy files), but this particular backend to work with zip archives
can be replaced easily by implementing only few required functions and compiling
against the new implementation.

To achieve the goal of supporting arbitrary structured arrays, ncr_numpy
includes a basic recursive descent parser (RDP) and a backtracking tokenizer.
Both the parser and the tokenizer can be used independently of ncr_numpy. Note,
however, that they do not support the full python formal grammar, but only the
subset required for ncr_numpy.


Reason
------
Existing implementations did not provide the functionality I needed. They are
also not robust enough and throw exceptions instead of return codes in library
routines, which I personally dislike (I try to never throw exceptions in library
code, which makes porting to or interfacing with other languages, such as plain
C, easier). In other parts, existing solutions make wrong assumptions which,
among other issues, leads to the support of only a narrow subset of the
capabilities of npy files.

For instance they are not correct regarding handling structured arrays of
arbitrary depth, or with data having mixed endianness, or in that they wrongly
assume that certain fields *must* be in the description string of a numpy array
and throw an exception, or that they don't even bother to check the itemsize
against the size of the file.  The list goes on.


Goals
-----
The goals of ncr_numpy is to achieve a robust, correct, and fast C++ library to
load and write numpy data from regular and compressed files, while supporting
not only basic types (numpy's built in types) but arbitrary structured arrays
such as nested structs. A further goal is to establish an API interface which is
easy to use, while also providing a low-level/verbose API which can further
reduce operations to access data.

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

There might be a proper paper, which describes the software in detail, to cite
in the future. So, stay tuned.

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
