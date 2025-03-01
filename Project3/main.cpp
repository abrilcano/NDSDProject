#include <fstream>
#include <iostream>
#include <cmath>
#include <mpi.h>

#include "Heat.hpp"

int main(int argc, char* argv[]) {

    // initialize MPI
    //Utilities::MPI::MPI_InitFinalize mpi_init(argc, argv);

    int id;
    int p;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &id);
    MPI_Comm_size(MPI_COMM_WORLD, &p);

    // if (id == 0){
    //     std::cout << "Heat equation solver using MPI" << std::endl;
    // }

    // Create an instance of the Heat class
    // alpha, dt, dx, L, threshold, maxSteps
    Heat heat(0.01, 0.001, 0.1, 0.8, 10e-4, 1, p, id);
    // Heat heat(0.01, 0.001, 0.1, 0.8, 10e-4, 1, 1, 0);
    
    heat.solve();

    //std::cout<<"MPI FINALIZE"<<std::endl;

    MPI_Finalize();

    return 0;
}