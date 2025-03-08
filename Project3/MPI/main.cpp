#include <fstream>
#include <iostream>
#include <cmath>
#include <mpi.h>

#include "Heat.hpp"

int main(int argc, char* argv[]) {

    int id;
    int p;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &id);
    MPI_Comm_size(MPI_COMM_WORLD, &p);

    // Create an instance of the Heat class with parametrs alpha, dt, dx, L, threshold, maxSteps
    Heat heat(0.01, 0.001, 0.1, 1.6, 10e-6, 100, p, id);
    heat.solve();

    MPI_Finalize();

    return 0;
}