#include "Heat.hpp"

#include <iostream>
#include <vector>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <mpi.h>

using namespace std;

// Function to initialize the temperature grid
void Heat::initializeGrid(double initialCornerTemp)
{
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            if ((i == 0 && j == 0) || (i == 0 && j == n - 1) ||
                (i == n - 1 && j == 0) || (i == n - 1 && j == n - 1))
            {
                grid[i][j] = initialCornerTemp; // Initialize corners
            }
            else
            {
                grid[i][j] = 100.0; // Initialize other points
            }
        }
    }
}

void Heat::initializeGhostValues()
{
    int nx = grid.size();
    int ny = grid[0].size();
    // cout << "initializeGhostValues" << endl;
    int val = 9 - id; 
    for (int i = 0; i < nx; ++i)
    {
        grid[i][0] = val;
        grid[i][ny-1] = val;
    }
    for (int j = 0; j < ny; ++j)
    {
        grid[0][j] = val;
        grid[nx-1][j] = val;
    }
}

// Function to initialize the corner temperatures
void Heat::initialCondition(double cornerTemp, int x, int y)
{
    int nx = grid.size();
    int ny = grid[0].size();

    for (int i = 1; i < nx -1; ++i)
    {
        for (int j = 1; j < ny-1; ++j)
        {
            grid[i][j] = id;
        }
    }

    if (x == 0 && y == 0)
    {
        grid[1][1] = 6;
    }
    if (x == px - 1 && y == 0)
    {
        grid[1][nx-2] = 7;
    }
    if (x == 0 && y == py - 1)
    {
        grid[nx - 2][1] = 8;
    }
    if (x == px - 1 && y == py - 1)
    {
        grid[nx-2][ny-2] = 9;
    }

}

void Heat::initialCondition(double cornerTemp, int startx, int endx, int starty, int endy)
{
    int nx = grid.size();
    int ny = grid[0].size();
    for (int i = 1; i < nx + 1; ++i)
    {
        for (int j = 1; j < ny + 1; ++j)
        {
            grid[i][j] = id;
        }
    }

    // cout << "startx: " << startx << " endx: " << endx << " starty: " << starty << " endy: " << endy << endl;
    // top left corner
    if (startx == 0 && starty == 0)
    {
        grid[1][1] = 6;
    }
    // top right corner
    if (endx == n - 1 && starty == 0)
    {
        grid[nx][1] = 7;
    }
    // bottom left corner
    if (startx == 0 && endy == n - 1)
    {
        grid[1][ny] = 8;
    }
    // bottom right corner
    if (endx == n - 1 && endy == n - 1)
    {
        grid[nx][ny] = 9;
    }
}

void Heat::printGrid()
{
    int nx = grid.size();
    int ny = grid[0].size();
    // logical print
    // std::cout << "   "; // For alignment of column headers
    // for (int j = 0; j < ny; ++j)
    // {
    //     std::cout << j << "   ";
    // }
    // std::cout << "\n";
    // for (int i = 0; i < nx; ++i)
    // {
    //     std::cout << i << " ";
    //     for (int j = 0; j < ny; ++j)
    //     {
    //         cout << fixed << setprecision(1) << grid[i][j] << " ";
    //     }
    //     cout << "\n";
    // }
    // Phisical print
    // std::cout << "   "; // For alignment of column headers
    // for (int x = 0; x < nx; ++x)
    // {
    //     std::cout << x << "   ";
    // }
    // std::cout << "\n";

    // for (int y = 0; y < ny; ++y) 
    // {
    //     std::cout << y << " "; // Print y-axis label
    //     for (int x = 0; x < nx; ++x)   // Iterate over the x-axis from left to right
    //     {
    //         std::cout << std::fixed << std::setprecision(1) << grid[x][y] << " ";
    //     }
    //     std::cout << "\n";
    // }

    for (int i = 0; i < nx; ++i)
    {
        for (int j = 0; j < ny; ++j)
        {
            cout << fixed << setprecision(1) << grid[i][j] << " ";
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
{
    for (int i = 0; i < n; ++i)
    {
        grid[0][i] = 10;
        grid[n - 1][i] = grid[n - 2][i];
        grid[i][0] = grid[i][1];
        grid[i][n - 1] = grid[i][n - 2];
    }
}

// Function to simulate heat diffusion
void Heat::solve()
{
    if (id == 0)
    {
        cout << "Heat equation solver using MPI" << endl;
        cout << "When dealing with " << n << " grid points per side and " << p << " processors." << endl;
        cout << "In the x direction px: " << px << " processors will be used and in the y direction py: " << py << " processors will be used" <<endl;
        // cout << "Each processor will have a: " << nx << " x " << ny << " points, represented in a "<< nx + 2 << " x " << ny + 2 <<" grid" << endl;
        cout << "------------------------------------------------------------------------------------------" << endl;
    }

    // Find local grid points

    int x = id % px;
    int y = floor(id / px);

    // int startx, starty, endx, endy;
    // if (x < rx)
    // {
    //     startx = x * (nx + 1);
    //     endx = startx + nx;
    // }
    // else
    // {
    //     startx = rx * (nx + 1) + (x - rx) * nx;
    //     endx = startx + nx - 1;
    // }
    // if (y < ry)
    // {
    //     starty = y * (ny + 1);
    //     endy = starty + ny;
    // }
    // else
    // {
    //     starty = ry * (ny + 1) + (y - ry) * ny;
    //     endy = starty + ny - 1;
    // }

    // Resize if the grid is not divisible by the number of processors
    if (rx != 0 && ry != 0 && x == px - 1 && y == py - 1)
    {
        nx = nx + ry;
        ny = ny + ry;
        grid.resize(nx + 2);
        for (auto& row : grid)
        {
            row.resize(ny + 2);
        }
    }
    if (rx != 0 && x == px - 1)
    {
        nx = nx + rx;
        grid.resize(nx + rx);
    }
    else if (ry != 0 && y == py - 1)
    {
        ny = ny + ry;
        for (auto& row : grid)
        {
            row.resize(ny + 2);
        }
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
    // cout << "Will have a grid of " << grid.size() << " x " << grid[0].size() << " points" << endl;
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

    if (id == 0)
    {
        cout << "Processor " << id << " after initialization: " << endl;
        printGrid();
    }
    if (id == 1)
    {
        cout << "Processor " << id << " after initialization: " << endl;
        printGrid();
    }
    // cout << "Processor " << id << " before exchange: " << endl;
    // printGrid();

    double dx2 = dx * dx;
    double alpha_dt_dx2 = alpha * dt / dx2;

    MPI_Barrier(MPI_COMM_WORLD);
    // cout << "Start simulation" << endl;

    // Create the column data type
    MPI_Datatype columnType;
    MPI_Type_vector(ny, 1, nx+1, MPI_DOUBLE, &columnType);
    MPI_Type_commit(&columnType);

    // Create the row data type
    MPI_Datatype rowType;
    MPI_Type_vector(nx, 1, ny, MPI_DOUBLE, &rowType);
    MPI_Type_commit(&rowType);


    for (int step = 0; step < maxSteps; ++step)
    {
        double maxChange = 0.0;

        // Exchange data between processors

        // Columns -------------------------

        //  Send data to the left
        if (neighbors[0] != -1)
        {
            //MPI_Send(&grid[1][1], ny, MPI_DOUBLE, neighbors[0], id, MPI_COMM_WORLD);
             MPI_Send(&grid[1][1], 1, columnType, neighbors[0], id, MPI_COMM_WORLD);
        }
        // Receive data from the right
        if (neighbors[2] != -1)
        {
            //MPI_Recv(&grid[nx + 1][1], ny, MPI_DOUBLE, neighbors[2], neighbors[2], MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&grid[1][nx + 1], 1, columnType, neighbors[2], neighbors[2], MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        // Send data to the right
        if (neighbors[2] != -1)
        {
            //MPI_Send(&grid[nx][1], ny, MPI_DOUBLE, neighbors[2], id, MPI_COMM_WORLD);
             MPI_Send(&grid[1][nx], 1, columnType, neighbors[2], id, MPI_COMM_WORLD);
        }
        // Receive data from the left
        if (neighbors[0] != -1)
        {
            //MPI_Recv(&grid[0][1], ny, MPI_DOUBLE, neighbors[0], neighbors[0], MPI_COMM_WORLD, MPI_STATUS_IGNORE);
             MPI_Recv(&grid[1][0], 1, columnType, neighbors[0], neighbors[0], MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        // Rows -------------------------

        // Send data to the bottom
        if (neighbors[3] != -1)
        {
            // MPI_Send(&grid[1][ny + 1], 1, rowType, neighbors[3], id, MPI_COMM_WORLD); 
            MPI_Send(&grid[ny][1], nx , MPI_DOUBLE, neighbors[3], id, MPI_COMM_WORLD);
        }
        // Receive data from the top
        if (neighbors[1] != -1)
        {
            // MPI_Recv(&grid[0][0], 1, rowType, neighbors[1], neighbors[1], MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&grid[0][1], nx, MPI_DOUBLE, neighbors[1], neighbors[1], MPI_COMM_WORLD, MPI_STATUS_IGNORE );
        }

        // Send data to the top
        if (neighbors[1] != -1)
        {
            // MPI_Send(&grid[1][1], 1, rowType, neighbors[1], id, MPI_COMM_WORLD);
            MPI_Send(&grid[1][1], nx, MPI_DOUBLE, neighbors[1], id, MPI_COMM_WORLD);
        }
        // Receive data from the bottom
        if (neighbors[3] != -1)
        {
            // MPI_Recv(&grid[1][ny + 1], 1, rowType, neighbors[3], neighbors[3], MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&grid[ny+1][1], nx, MPI_DOUBLE, neighbors[3], neighbors[3], MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        MPI_Barrier(MPI_COMM_WORLD);

        if (id == 0)
        {
            cout << "Processor " << id << " after exchange: " << endl;
            printGrid();
        }
        if (id == 1)
        {
            cout << "Processor " << id << " after exchange: " << endl;
            printGrid();
        }

        // cout << "Processor " << id << " after exchange: " << endl;
        // printGrid();

        // Compute new temperature values
        //TODO
    }

    MPI_Type_free(&columnType);
    MPI_Type_free(&rowType);

}

/*
Processor 2 after exchange:
   0   1   2   3   4   5
0 5.0 0.0 0.0 0.0 0.0 5.0
1 5.0 2.0 2.0 2.0 2.0 3.0
2 5.0 2.0 2.0 2.0 2.0 3.0
3 5.0 2.0 2.0 2.0 2.0 3.0
4 5.0 8.0 2.0 2.0 2.0 3.0
5 5.0 5.0 5.0 5.0 5.0 5.0
Processor 3 after exchange:
   0   1   2   3   4   5
0 6.0 1.0 1.0 1.0 1.0 6.0
1 2.0 3.0 3.0 3.0 3.0 6.0
2 2.0 3.0 3.0 3.0 3.0 6.0
3 2.0 3.0 0.0 3.0 3.0 6.0
4 2.0 3.0 3.0 3.0 9.0 6.0
5 6.0 6.0 6.0 6.0 6.0 6.0
Processor 0 after exchange:
   0   1   2   3   4   5
0 3.0 3.0 3.0 3.0 3.0 3.0
1 3.0 6.0 0.0 0.0 0.0 1.0
2 3.0 0.0 0.0 0.0 0.0 1.0
3 3.0 0.0 0.0 0.0 0.0 1.0
4 3.0 0.0 0.0 0.0 0.0 1.0
5 3.0 2.0 2.0 2.0 2.0 3.0
Processor 1 after exchange:
   0   1   2   3   4   5
0 4.0 4.0 3.0 4.0 4.0 4.0
1 0.0 1.0 1.0 1.0 7.0 4.0
2 0.0 1.0 1.0 1.0 1.0 4.0
3 0.0 1.0 0.0 1.0 1.0 4.0
4 0.0 1.0 1.0 1.0 1.0 4.0
5 4.0 3.0 3.0 3.0 3.0 4.0
*/

/*
Expected output:
Processor 0 after exchange:
   0   1   2   3   4   5   6   7   8   9
0 3.0 3.0 3.0 3.0 3.0 3.0 3.0 3.0 3.0 3.0
1 3.0 6.0 0.0 0.0 0.0 0.0 0.0 0.0 7.0 3.0
2 3.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 3.0
3 3.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 3.0
4 3.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 3.0
5 3.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0 3.0

   0   1   2   3   4   5   6   7   8   9
Processor 1 after exchange:
0 4.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 4.0
1 4.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0 4.0
2 4.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0 4.0
3 4.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0 4.0
4 4.0 8.0 1.0 1.0 1.0 1.0 1.0 1.0 9.0 4.0
5 4.0 4.0 4.0 4.0 4.0 4.0 4.0 4.0 4.0 4.0


Obtained output:
Processor 0 after exchange:
   0   1   2   3   4   5   6   7   8   9
0 3.0 3.0 8.0 3.0 3.0 3.0 3.0 3.0 3.0 3.0
1 3.0 6.0 4.0 0.0 0.0 0.0 0.0 0.0 7.0 3.0
2 3.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 3.0
3 3.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 3.0
4 3.0 0.0 4.0 0.0 0.0 0.0 0.0 0.0 0.0 3.0
5 3.0 1.0 3.0 3.0 3.0 3.0 3.0 3.0 3.0 3.0

Processor 1 after exchange:
   0   1   2   3   4   5   6   7   8   9
0 3.0 4.0 4.0 4.0 4.0 4.0 4.0 4.0 4.0 4.0
1 0.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0 4.0
2 0.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0 4.0
3 3.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0 4.0
4 0.0 8.0 1.0 1.0 1.0 1.0 1.0 1.0 9.0 4.0
5 0.0 4.0 4.0 4.0 4.0 4.0 4.0 4.0 4.0 4.0
I*/