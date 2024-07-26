To Do
=====
general: provide allocation-free interface wherever possible
general: iterator support during writes (input iterator)
general: * iterator support during reads (output iterator)
         * output iterator also for loading (to circument ndarray):
              ncr::numpy::load('file.npy', iterator);
           -> this should write dtype'd items to the iterator

npy: load simple npy from non-seekable IO stream (TCP, named pipe)
npy: load simple npz from non-seekable IO stream (TCP, named pipe)
npy: support Eigen matrices / arrays as backend

ndarray: implement range views for basic types (i32, i64, f32, f64, etc) that uses ndarray's get function
ndarray: implement slicing
ndarray: re-evaluate the design decision regarding ndarray_item
ndarray: evaluate where temporaries are created and try to avoid them. If unavoidable, clearly document them


Done
====
