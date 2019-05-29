# adios2_C
ADIOS2 C examples


Requires a location with installed adios2 library (after make install) and cmake 3.6 or above

To build:

- `mkdir build`
- `cd build`
- `cmake -DADIOS2_DIR=/path/to/adios2/lib/cmake/adios2 ..`
- `make`

To run (from inside build):
- `mpirun -n 2 examples/writer_weak_scaling.exe 4 8`