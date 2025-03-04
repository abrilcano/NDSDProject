#include "Heat.hpp"

#include <iostream>
#include <vector>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <mpi.h>

using namespace std;

int::Heat::get(int i, int j)
{
    return i * (nx + 2) + j;
}

// Function to initialize the temperature grid
void Heat::initializeGrid(double initialCornerTemp)
{}

void Heat::initializeGhostValues()
{

    // cout << "initializeGhostValues" << endl;
    int val = 9 - id; 
    for (int i = 0; i < nx + 2; ++i)
    {
        contiguosGrid[i * (nx+2) + 0] = val;
        contiguosGrid[i * (nx+2) + (ny+1)] = val;
    }
    for (int j = 0; j < ny + 2 ; ++j)
    {
        contiguosGrid[0 * (nx+2) + j] = val;
        contiguosGrid[(nx+1) * (nx+2) + j] = val;
    }
}

// Function to initialize the corner temperatures
void Heat::initialCondition(double cornerTemp, int x, int y)
{

    for (int i = 1; i < nx + 1; ++i)
    {
        for (int j = 1; j < ny + 1; ++j)
        {
            contiguosGrid[i * (nx+2) + j] = id;
        }
    }

    if (x == 0 && y == 0)
    {
        contiguosGrid[1 * (nx+2) + 1] = 6;
    }
    if (x == px - 1 && y == 0)
    {
        contiguosGrid[1 * (nx+2) + (nx)] = 7;
    }
    if (x == 0 && y == py - 1)
    {
        contiguosGrid[(nx) * (nx+2) + 1] = 8;
    }
    if (x == px - 1 && y == py - 1)
    {
        contiguosGrid[(nx)* (nx+2) + (ny)] = 9;
    }

}


void Heat::printGrid()
{

    for (int i = 0; i < nx + 2; ++i)
    {
        for (int j = 0; j < ny + 2; ++j)
        {
            // cout << fixed << setprecision(1) << grid[get[i,j)] << " ";
            cout << contiguosGrid[get(i, j)] << " ";
        }
        cout << "\n";
    }
}

// Function to save the temperature grid to a file
void Heat::output(int timeStep)
{
    ofstream file("temperature_step_" + to_string(timeStep) + ".txt");
    if (!file)
    {
        cerr << "Error opening file for writing.\n";
        return;
    }

    for (const auto &row : grid)
    {
        for (const auto &temp : row)
        {
            file << fixed << setprecision(4) << temp << " ";
        }
        file << "\n";
    }
    file.close();
}

void Heat::applyBoundaryConditions()
{}

// Function to simulate heat diffusion
void Heat::solve()
{
    if (id == 0)
    {
        cout << "Heat equation solver using MPI" << endl;
        cout << "When dealing with " << n << " grid points per side and " << p << " processors." << endl;
        cout << "In the x direction px: " << px << " processors will be used and in the y direction py: " << py << " processors will be used" <<endl;
        cout << "Each processor will have a: " << nx << " x " << ny << " points, represented in a "<< nx + 2 << " x " << ny + 2 <<" grid" << endl;
        cout << "------------------------------------------------------------------------------------------" << endl;
    }

    // Find local grid points

    int x = id % px;
    int y = floor(id / px);

    // Resize if the grid is not divisible by the number of processors
    // Testing needed
    if (rx != 0 && ry != 0 && x == px - 1 && y == py - 1)
    {
        nx = nx + ry;
        ny = ny + ry;
        contiguosGrid.resize((nx + 2) * (ny + 2));
        contiguosNewGrid.resize((nx + 2) * (ny + 2));

    }
    if (rx != 0 && x == px - 1)
    {
        nx = nx + rx;
        contiguosGrid.resize((nx + 2) * (ny + 2));
        contiguosNewGrid.resize((nx + 2) * (ny + 2));
    }
    else if (ry != 0 && y == py - 1)
    {
        ny = ny + ry;
        contiguosGrid.resize((nx + 2) * (ny + 2));
        contiguosNewGrid.resize((nx + 2) * (ny + 2));
    }

    // array of neighbors
    int neighbors[4] = {-1, -1, -1, -1};

    // find neighbors in clockwise order starting from the left

    // left neighbor
    if (x > 0)
    {
        neighbors[0] = id - 1;
    }
    // top neighbor
    if (y > 0)
    {
        neighbors[1] = id - px;
    }
    // right neighbor
    if (x < px - 1)
    {
        neighbors[2] = id + 1;
    }
    // bottom neighbor
    if (y < py - 1)
    {
        neighbors[3] = id + px;
    }

    // cout << "--------------------------------" << endl;
    // cout << "Processor " << id << " is at position: " << x << ", " << y << endl;
    // cout << "Will have a grid of " << nx + 2  << " x " << ny + 2 << " points" << endl;
    // cout << "Will process: " << endl;
    // cout << "x: " << startx << " to " << endx << endl;
    // cout << "y: " << starty << " to " << endy << endl;
    // cout << "Neighbors of processor " << id << " are: ";
    // for (int j = 0; j < 4; j++)
    // {
    //     cout << neighbors[j] << " ";
    // }
    // cout << endl;
    // cout << "--------------------------------" << endl;


    initializeGhostValues();
    // initialCondition(10.0, startx, endx, starty, endy);
    initialCondition(10.0, x, y);

    // if (id == 1)
    // {
    //     cout << "Processor " << id << " after initialization: " << endl;
    //     printGrid();
    // }
    cout << "Processor " << id << " before exchange: " << endl;
    printGrid();

    double dx2 = dx * dx;
    double alpha_dt_dx2 = alpha * dt / dx2;

    MPI_Barrier(MPI_COMM_WORLD);
    // cout << "Start simulation" << endl;

    // Create the column data type
    MPI_Datatype columnType;
    MPI_Type_vector(ny, 1, nx + 2, MPI_DOUBLE, &columnType);
    MPI_Type_commit(&columnType);


    for (int step = 0; step < maxSteps; ++step)
    {
        double maxChange = 0.0;

        // Exchange data between processors

        // Columns -------------------------

        //  Send data to the left
        if (neighbors[0] != -1)
        {
            //MPI_Send(&grid[1][1], ny, MPI_DOUBLE, neighbors[0], id, MPI_COMM_WORLD);
            MPI_Send(&contiguosGrid[1 * (nx + 2) + 1], 1, columnType, neighbors[0], id, MPI_COMM_WORLD);
        }
        // Receive data from the right
        if (neighbors[2] != -1)
        {
            //MPI_Recv(&grid[nx + 1][1], ny, MPI_DOUBLE, neighbors[2], neighbors[2], MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&contiguosGrid[1 * (nx + 2) + (nx + 1)], 1, columnType, neighbors[2], neighbors[2], MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        // Send data to the right
        if (neighbors[2] != -1)
        {
            //MPI_Send(&grid[nx][1], ny, MPI_DOUBLE, neighbors[2], id, MPI_COMM_WORLD);
            MPI_Send(&contiguosGrid[1 * (nx + 2) + nx], 1, columnType, neighbors[2], id, MPI_COMM_WORLD);
        }
        // Receive data from the left
        if (neighbors[0] != -1)
        {
            //MPI_Recv(&grid[0][1], ny, MPI_DOUBLE, neighbors[0], neighbors[0], MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&contiguosGrid[1 * (nx+2) + 0], 1, columnType, neighbors[0], neighbors[0], MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        // Rows -------------------------

        // Send data to the bottom
        if (neighbors[3] != -1)
        {
            MPI_Send(&contiguosGrid[ny * (nx + 2) + 1], nx , MPI_DOUBLE, neighbors[3], id, MPI_COMM_WORLD);
        }
        // Receive data from the top
        if (neighbors[1] != -1)
        {
            MPI_Recv(&contiguosGrid[0 * (nx + 2) + 1], nx, MPI_DOUBLE, neighbors[1], neighbors[1], MPI_COMM_WORLD, MPI_STATUS_IGNORE );
        }

        // Send data to the top
        if (neighbors[1] != -1)
        {
            MPI_Send(&contiguosGrid[1 * (nx + 2) + 1], nx, MPI_DOUBLE, neighbors[1], id, MPI_COMM_WORLD);
        }
        // Receive data from the bottom
        if (neighbors[3] != -1)
        {
            MPI_Recv(&contiguosGrid[(ny + 1) * (nx + 2) + 1], nx, MPI_DOUBLE, neighbors[3], neighbors[3], MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        MPI_Barrier(MPI_COMM_WORLD);

        // if (id == 0)
        // {
        //     cout << "Processor " << id << " after exchange: " << endl;
        //     printGrid();
        // }

        cout << "Processor " << id << " after exchange: " << endl;
        printGrid();

        // Compute new temperature values
        //TODO
    }

    MPI_Type_free(&columnType);

}
