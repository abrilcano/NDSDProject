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

    // Create an instance of the Heat class
    // alpha, dt, dx, L, threshold, maxSteps
    Heat heat(0.01, 0.001, 0.01, 1.0, 10e-6, 40000, p, id);
    
    heat.solve();

    //std::cout<<"MPI FINALIZE"<<std::endl;

    MPI_Finalize();

    return 0;
}