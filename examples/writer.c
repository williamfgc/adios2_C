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

int main(int argc, char **argv)
{
    int error_return = 0;
    error_return = MPI_Init(&argc, &argv);
    if (error_return != MPI_SUCCESS)
    {
        printf("Writer: ERROR: Unable to initialize MPI\n");
        return 1;
    }

    if (argc != 7)
    {
        printf("Writer: ERROR: need size, start and count parameters\n");
        printf("Example: \n");
        printf("    mpirun -n 4 writer 0 4 0 4\n\n");
        printf("    Size, start, and count for x first and then y");
        return 1;
    }
    // parse nrows ncolumns
    const int xsize = atoi(argv[1]);
    const int xstart = atoi(argv[2]);
    const int xcount = atoi(argv[3]);
    const int ysize = atoi(argv[4]);
    const int ystart = atoi(argv[5]);
    const int ycount = atoi(argv[6]);
    const int nelements = xsize * ysize;

    // get size, BTW we hardly ever use MPI_COMM_WORLD in Apps
    // for the example is fine
    int size;
    error_return = MPI_Comm_size(MPI_COMM_WORLD, &size);
    if (error_return != MPI_SUCCESS)
    {
        printf("Writer: ERROR: Unable to get MPI size\n");
        return 1;
    }
    // get current rank, it will be used to determine each block start
    int rank = 0;
    error_return = MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (error_return != MPI_SUCCESS)
    {
        printf("Writer: ERROR: Unable to get MPI rank\n");
        return 1;
    }

    size_t shape[2];
    size_t start[2];
    size_t count[2];

    // Global dimensions (count)
    // We know the global shape
    shape[0] = (size_t)xsize;
    shape[1] = (size_t)ysize;

    // Global offset (start)
    start[0] = xstart;
    start[1] = ystart;

    // Local dimensions (count)
    count[0] = (size_t)xcount;
    count[1] = (size_t)ycount;

    // ADIOS2 initialize part
    adios2_error adiosErr;
    adios2_adios *adios = adios2_init(MPI_COMM_WORLD, adios2_debug_mode_on);
    adios2_io *io = adios2_declare_io(adios, "BPFile_Write2");
    adiosErr = adios2_set_engine(io, "bp3");

    // it's very nice to do error checking
    if (adiosErr != adios2_error_none)
    {
        printf("Writer: ERROR: set_engine failed: %d\n", adiosErr);
        adios2_finalize(adios);
        MPI_Finalize();
        return 3;
    }

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
    for (i = 0; i < xcount; ++i)
    {
        // columns: faster index j
        for (j = 0; j < ycount; ++j)
        {
            const size_t index = i * ycount + j;
            my_array[index] = (xstart + i) * ysize + ystart + j;
            printf("%d ", my_array[index]);
        }
        printf("\n");
    }

    // use adios to transport your variables
    adios2_engine *engine = adios2_open(io, "example", adios2_mode_write);

    adiosErr = adios2_put(engine, var_rows, &xsize, adios2_mode_deferred);
    if (adiosErr != adios2_error_none)
        printf("There was a put error in total_rows.\n");

    adiosErr = adios2_put(engine, var_columns, &ysize, adios2_mode_deferred);
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
