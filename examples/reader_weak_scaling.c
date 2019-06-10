/*
 * reader.c
 *
 *  Created on: Jun 9, 2019
 *      Author: William F Godoy godoywf@ornl.gov
 */

#include <stdio.h>
#include <stdlib.h>

#include <adios2_c.h>
#include <mpi.h>

static void myCheck(int error_return, const char *message)
{
    if (error_return != 0)
    {
        printf("Writer: ERROR: %s\n", message);
        MPI_Finalize();
        exit(error_return);
    }
}

int main(int argc, char **argv)
{
    int error_return = 0;

    myCheck(MPI_Init(&argc, &argv), "Unable to initialize MPI");

    int wrank, wsize;
    myCheck(MPI_Comm_rank(MPI_COMM_WORLD, &wrank), "Unable to get wrank");
    myCheck(MPI_Comm_size(MPI_COMM_WORLD, &wsize), "Unable to get wsize");

    // Define I/O communicator,
    // basically a copy of MPI_COMM_WORLD
    // since all ranks point to the same color
    const int color = 1;
    MPI_Comm ioComm;
    myCheck(MPI_Comm_split(MPI_COMM_WORLD, color, wrank, &ioComm),
            " in I/O communicator");

    int rank, size;
    // get current rank, it will be used to determine each block start
    myCheck(MPI_Comm_rank(ioComm, &rank), "Unable to get I/O Comm MPI rank");
    // get current size, it will be used to determine total shape
    myCheck(MPI_Comm_size(ioComm, &size), "Unable to get I/O Comm MPI size");

    // Initialize adios2
    adios2_adios *adios = adios2_init(ioComm, adios2_debug_mode_on);
    adios2_io *io = adios2_declare_io(adios, "BPFile_Read");
    myCheck(adios2_set_engine(io, "bp3"), "adios2_set_engine failed:");

    // open engine
    adios2_engine *engine = adios2_open(io, "example.bp", adios2_mode_read);

    adios2_variable *var_array = adios2_inquire_variable(io, "my_array");

    if (var_array == NULL)
    {
        myCheck(1, "variable my_array not found");
    }

    size_t shape[2];
    adios2_variable_shape(shape, var_array);

    // create compute comm
    int dims[2] = {};
    // define cartesian grid to define task
    myCheck(MPI_Dims_create(wsize, 2, dims), "Unable to create Dims partition");
    const int periods[2] = {1, 1};
    MPI_Comm cartComm2D;
    myCheck(MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &cartComm2D),
            "Unable to create Cart Comm");

    // get offsets in cart coordinates
    int coords[2] = {};
    myCheck(MPI_Cart_coords(cartComm2D, rank, 2, coords),
            "Unable to get Cart coords");

    size_t xCount = shape[0] / dims[0];
    size_t yCount = shape[1] / dims[1];

    // Global offset (start)
    size_t start[2];
    start[0] = (size_t)(coords[0] * xCount);
    start[1] = (size_t)(coords[1] * yCount);

    // Local dimensions (count)
    size_t count[2];
    count[0] = (size_t)xCount;
    count[1] = (size_t)yCount;

    printf("Rank %d : Shape: %d %d Count: %d %d  Start: %d %d \n", wrank,
           shape[0], shape[1], count[0], count[1], start[0], start[1]);

    size_t nelements = count[0] * count[1];
    int *my_array = (int *)calloc(nelements, sizeof(int));

    adios2_set_selection(var_array, 2, start, count);
    adios2_get(engine, var_array, my_array, adios2_mode_deferred);
    adios2_close(engine);
    adios2_finalize(adios);

    // inspect my_array
    const size_t buffer_size = nelements * (sizeof(int) + 1) + yCount * 2 + 200;
    char *buffer = (char *)calloc(buffer_size, 1);

    size_t pos = 0;

    for (size_t i = 0; i < xCount; ++i)
    {
        // columns: faster index j
        for (size_t j = 0; j < yCount; ++j)
        {
            const size_t index = i * yCount + j;

            sprintf(&buffer[pos], "%4d ", my_array[index]);
            pos += 5;
        }
    }

    printf("rank %d : %s\n", wrank, buffer);
    free(buffer);
    free(my_array);
    MPI_Finalize();
    return 0;
}
