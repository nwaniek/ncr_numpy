cmake_minimum_required(VERSION 3.23)

# ncr_numpy's version is written in ../VERSION, let's read it and map it into
# major and minor parts so that we can use it
file(READ "${CMAKE_SOURCE_DIR}/../VERSION" VERSION_FILE)
string(STRIP "${VERSION_FILE}" NCR_NUMPY_VERSION)
string(REPLACE "." ";" NCR_NUMPY_VERSION_LIST "${NCR_NUMPY_VERSION}")
list(GET NCR_NUMPY_VERSION_LIST 0 NCR_NUMPY_VERSION_MAJOR)
list(GET NCR_NUMPY_VERSION_LIST 1 NCR_NUMPY_VERSION_MINOR)


project(
	example
	VERSION ${NCR_NUMPY_VERSION_MAJOR}.${NCR_NUMPY_VERSION_MINOR}
	DESCRIPTION "Example ncr_numpy project"
	LANGUAGES CXX)

# the example project consists of an application 'example', whose source code
# can be found in example.cpp. It also uses a zip-backend, for which it uses
# ncr_numpy's default backend implementation
add_executable(example example.cpp)

# pass along version information
target_compile_definitions(
	example PRIVATE
	VERSION_MAJOR=${NCR_NUMPY_VERSION_MAJOR}
	VERSION_MINOR=${NCR_NUMPY_VERSION_MINOR}
	VERSION="${NCR_NUMPY_VERSION}"
)

# the target needs to find the 'ncr' directory. Usually you will have copied
# that to some other location, e.g. it might reside under /opt/ncr_numpy/ncr, so
# you would have to point the build system to /opt/ncr_numpy instead of '..'
target_include_directories(example PUBLIC ..)

# for the example above which uses ncr_numpy's default zip backend, we need to
# link against libzip
target_link_libraries(example PUBLIC zip)

# finally, ncr_numpy uses C++20 features
target_compile_features(example PUBLIC cxx_std_20)
