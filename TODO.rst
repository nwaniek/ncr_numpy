To Do
=====
This file contains TODO items for the different major elements of ncr_numpy.
Note that ncr_numpy as used below referes to the header ncr_numpy.hpp. Moreover,
there are more fine-grained TODO items noted directly in the source files.


ncr_numpy
---------
* load simple npy from non-seekable IO stream (TCP, named pipe)
* load simple npz from non-seekable IO stream (TCP, named pipe)
* support Eigen matrices / arrays


ncr_ndarray
-----------
* implement range views for basic types (i32, i64, f32, f64, etc) that uses ndarray's get function
* implement slicing
* re-evaluate the design decision regarding ndarray_item
* evaluate where temporaries are created and try to avoid them. If unavoidable, clearly document them


ncr_pyparser
------------


ncr_bits
--------


ncr_types
---------
