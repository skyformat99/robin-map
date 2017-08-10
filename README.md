## A C++ implementation of a fast hash map using robin hood hashing

**The library is still in alpha stage.**

Two classes are provided: `tsl::robin_map` and `tsl::robin_set`.


### Installation
To use robin-map, just add the [src/](src/) directory to your include path. It is a **header-only** library.

The code should work with any C++11 standard-compliant compiler and has been tested with GCC 4.8.4, Clang 3.5.0 and Visual Studio 2015.

To run the tests you will need the Boost Test library and CMake.

```bash
git clone https://github.com/Tessil/robin-map.git
cd robin-map
mkdir build
cd build
cmake ..
make
./test_robin_map
```

### License

The code is licensed under the MIT license, see the [LICENSE file](LICENSE) for details.