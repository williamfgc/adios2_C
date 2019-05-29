/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 *
 * writer.c : The goal of this program is to use ADIOS2 to be able to have
 * multiple processes to be able to write to one single file in blocks.
 *
 *  Modified on: May 28, 2019
 *      Author: Amanda Hines
 *              Aaron Valoroso
 *              William F Godoy godoywf@ornl.gov
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

    if (argc != 3)
    {
        printf("Writer: ERROR: need xCount and yCount parameters\n");
        printf("Example: \n");
        printf("    mpirun -n 4 writer xCount yCount\n");
        printf("    xCount and yCount constant dimensions per rank");
        printf("    Load per rank = xCount * yCount * sizeof(int)");
        printf("    Total load = xCount * yCount * sizeof(int) * nprocs");
        return 1;
    }
    // local nrows ncolumns
    const int xCount = atoi(argv[1]);
    const int yCount = atoi(argv[2]);
    const int nelements = xCount * yCount;

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

    printf("Dims: %d %d %d\n", dims[0], dims[1], rank);
    printf("Coords: %d %d %d\n", coords[0], coords[1], rank);

    size_t shape[2];
    size_t start[2];
    size_t count[2];

    // Global dimensions (might not be the most efficient partition)
    shape[0] = (size_t)(dims[0] * xCount);
    shape[1] = (size_t)(dims[1] * yCount);

    // Global offset (start)
    start[0] = (size_t)(coords[0] * xCount);
    start[1] = (size_t)(coords[1] * yCount);

    // Local dimensions (count)
    count[0] = (size_t)xCount;
    count[1] = (size_t)yCount;

    // ADIOS2 initialize part
    adios2_error adiosErr;
    adios2_adios *adios = adios2_init(MPI_COMM_WORLD, adios2_debug_mode_on);
    adios2_io *io = adios2_declare_io(adios, "BPFile_Write2");
    myCheck(adios2_set_engine(io, "bp3"), "adios2_set_engine failed:");

    // NULL dimensions since it's a single global value
    // pick meaningful names for your variable handlers
    // use fix width types e.g.: adios2_type_int32_t
    // we are moving to those adios2_type_int

    // Global Single value variables so pass NULL to dimensions
    adios2_variable *var_rows =
        adios2_define_variable(io, "total_rows", adios2_type_int32_t, 2, NULL,
                               NULL, NULL, adios2_constant_dims_true);
    adios2_variable *var_columns =
        adios2_define_variable(io, "total_columns", adios2_type_int32_t, 2,
                               NULL, NULL, NULL, adios2_constant_dims_true);

    // Global Array variable, here pass the shape, start, offset
    adios2_variable *var_array =
        adios2_define_variable(io, "my_array", adios2_type_int32_t, 2, shape,
                               start, count, adios2_constant_dims_true);

    // set your data (usually this comes from your application code, numerical
    // solver, etc.
    int *my_array = (int *)calloc(nelements, sizeof(int));
    // populate your pre-allocate data
    // rows: slower index i
    printf("This is what will be stored in the array...\n");
    int i, j;
    for (i = 0; i < xCount; ++i)
    {
        // columns: faster index j
        for (j = 0; j < yCount; ++j)
        {
            const size_t index = i * yCount + j;
            my_array[index] = (start[0] + i) * shape[1] + start[1] + j;
            printf("%d ", my_array[index]);
        }
        printf("\n");
    }

    // use adios to transport your variables
    adios2_engine *engine = adios2_open(io, "example", adios2_mode_write);

    adiosErr = adios2_put(engine, var_rows, &dims[0], adios2_mode_deferred);
    if (adiosErr != adios2_error_none)
        printf("There was a put error in total_rows.\n");

    adiosErr = adios2_put(engine, var_columns, &dims[1], adios2_mode_deferred);
    if (adiosErr != adios2_error_none)
        printf("There was a put error in total_columns.\n");

    // write a piece to shape -> from the (start, count) window
    adiosErr = adios2_put(engine, var_array, my_array, adios2_mode_deferred);
    if (adiosErr != adios2_error_none)
        printf("There was a put error in my_array.\n");

    // since we used deferred mode the actual writing of the 3 variables is done
    // at close
    adios2_close(engine);
    adios2_finalize(adios);

    free(my_array);
    MPI_Finalize();
    return 0;
}
