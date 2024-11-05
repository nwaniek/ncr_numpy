#!/usr/bin/env python

import numpy as np

# simple array
np.save('in/simple.npy', np.array([[1,2,3],[4,5,6]]))
np.savez('in/simple.npz', simple_array=np.array([[1,2,3],[4,5,6]]))

# simple tensors
shape = (2, 3, 5)
np.save('in/simpletensor1.npy', np.ones(shape))
np.savez('in/simpletensor1.npz', np.ones(shape))

shape = (2, 5, 7)
np.save('in/simpletensor2.npy', np.arange(np.prod(shape)).reshape(shape))
np.savez('in/simpletensor2.npz', tensor=np.arange(np.prod(shape)).reshape(shape))

# complex numbers
np.save('in/complex.npy', np.ones((3,3), dtype='>c8'))
np.savez('in/complex.npz', np.ones((3,3), dtype='>c8'))

# structured array
dt_structured = np.dtype([('name', np.str_, 16), ('grades', np.float64, (2,))])
x = np.array([('Sarah', (8.0, 7.0)), ('John', (6.0, 7.0))], dtype=dt_structured)
np.save('in/structured.npy', x)
np.savez('in/structured.npz', structured_array=x)

# nested structured array
dt_nested = np.dtype([('year', np.uint32),
                      ('countries', [
                          ('c1', [('country', np.str_, 16), ('gdp', np.uint64)]),
                          ('c2', [('country', np.str_, 16), ('gdp', np.uint64)]),
                          ('c3', [('country', np.str_, 16), ('gdp', np.uint64)])])])
arr = np.array([(2023, (('USA', 27360935), ('China', 17794782), ('Germany', 4456081))),
                (2022, (('USA', 25744100), ('China', 17963170), ('Germany', 4076923)))], dtype=dt_nested)
np.save('in/nested.npy', arr)

# compressed npz with multiple files
np.savez_compressed("in/multiple_unnamed.npz", np.ones((2,2)), np.ones((4,4)), np.ones((6,6)))
np.savez_compressed("in/multiple_named.npz", one=np.ones((2,2)), two=np.ones((4,4)), three=np.ones((6,6)))

