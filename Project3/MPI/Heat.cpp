#include "Heat.hpp"

#include <iostream>
#include <vector>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <mpi.h>

using namespace std;

int Heat::get(int i, int j)
{
    return i * (nx + 2) + j;
}

void Heat::initializeGhostValues()
{

    // cout << "initializeGhostValues" << endl;
    // int val = id-9; 
    int val = 0;
    for (int i = 0; i < nx + 2; ++i)
    {
        grid[get(i,0)] = val;
        grid[get (i, ny + 1)] = val;
    }
    for (int j = 0; j < ny + 2 ; ++j)
    {
        grid[get(0,j)] = val;
        grid[get(nx + 1, j)] = val;
    }
}

void Heat::initialCondition(double temp, int x, int y)
{

    for (int i = 1; i < nx + 1; ++i)
    {
        for (int j = 1; j < ny + 1; ++j)
        {
            // grid[get(i,j)] = id;
            grid[get(i,j)] = 0.0; // Initialize to zero
        }
    }

    // Set the corner temperatures 
    if (x == 0 && y == 0)
    {
        grid[get(1,1)] = temp;
    }
    if (x == px - 1 && y == 0)
    {
        grid[get(1,nx)] = temp;
    }
    if (x == 0 && y == py - 1)
    {
        grid[get(nx,1)] = temp;
    }
    if (x == px - 1 && y == py - 1)
    {
        grid[get(nx,ny)] = temp;
    }

}

void Heat::applyBoundaryConditions(double temp, int x, int y)
{

    if (x == 0 && y == 0)
    {
        grid[get(1,1)] = temp;
    }
    if (x == px - 1 && y == 0)
    {
        grid[get(1,nx)] = temp;
    }
    if (x == 0 && y == py - 1)
    {
        grid[get(nx,1)] = temp;
    }
    if (x == px - 1 && y == py - 1)
    {
        grid[get(nx,ny)] = temp;
    }

}

void Heat::printGrid()
{

    for (int i = 0; i < nx + 2; ++i)
    {
        for (int j = 0; j < ny + 2; ++j)
        {
            // cout << fixed << setprecision(1) << grid[get[i,j)] << " ";
            cout << grid[get(i, j)] << " ";
        }
        cout << "\n";
    }
}

void Heat::writeVTK(const std::string &filename)
{
    std::ofstream file(filename);
    if (!file)
    {
        std::cerr << "Error: Unable to open file " << filename << " for writing." << std::endl;
        return;
    }

    file << "# vtk DataFile Version 3.0\n";
    file << "2D Heat Diffusion Output\n";
    file << "ASCII\n";
    file << "DATASET STRUCTURED_POINTS\n";
    file << "DIMENSIONS " << nx << " " << ny << " 1\n";
    file << "ORIGIN 0 0 0\n";  // Adjust if needed
    file << "SPACING " << dx << " " << dx << " 1\n"; // Uniform grid spacing
    file << "POINT_DATA " << nx * ny << "\n";
    file << "SCALARS temperature double 1\n";
    file << "LOOKUP_TABLE default\n";

    for (int j = 1; j < ny + 1; ++j)
    {
        for (int i = 1; i < nx + 1; ++i)
        {
            file << grid[get(i, j)] << " ";
        }
        file << "\n";
    }

    file.close();
    std::cout << "VTK file written: " << filename << std::endl;
}

void Heat::writeVTKParallel(int timeStep, int x, int y)
{
    std::string filename = "../outputVTK/heat_diffusion" + to_string(timeStep) + ".vtk";

    if (id == 0)
    {
        std::remove(filename.c_str());
    }

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_File file;
    MPI_File_open(MPI_COMM_WORLD, filename.c_str(), MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &file);

    MPI_Offset offsetH = 0;

    // Write VTK header (only processor 0)
    if (id == 0)
    {
        stringstream header;
        header << "# vtk DataFile Version 3.0\n";
        header << "2D Heat Diffusion Output\n";
        header << "ASCII\n";
        header << "DATASET STRUCTURED_POINTS\n";
        header << "DIMENSIONS " << (px * nx) << " " << (py * ny) << " 1\n";
        header << "ORIGIN 0 0 0\n";
        header << "SPACING " << dx << " " << dx << " 1\n";
        header << "POINT_DATA " << (px * nx * py * ny) << "\n";
        header << "SCALARS temperature double 1\n";
        header << "LOOKUP_TABLE default\n";

        MPI_File_write_at(file, offsetH, header.str().c_str(), header.str().size(), MPI_CHAR, MPI_STATUS_IGNORE);
        offsetH = header.str().size();
    }

    // Broadcast header size to all processors
    MPI_Bcast(&offsetH, 1, MPI_OFFSET, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);

    // Collect all data from all processors to processor 0
    int total_points = px * nx * py * ny;
    vector<double> global_data;
    
    if (id == 0) {
        global_data.resize(total_points);
    }

    // Each processor sends its data to processor 0
    vector<double> local_data(nx * ny);
    int idx = 0;
    for (int j = 1; j < ny + 1; ++j) {
        for (int i = 1; i < nx + 1; ++i) {
            local_data[idx++] = grid[get(i, j)];
        }
    }

    // Gather all local data to processor 0
    vector<int> recvcounts(p);
    vector<int> displs(p);
    
    if (id == 0) {
        for (int proc = 0; proc < p; ++proc) {
            int proc_x = proc % px;
            int proc_y = proc / px;
            
            // Handle remainder grid points
            int proc_nx = nx;
            int proc_ny = ny;
            if (proc_x == px - 1 && rx != 0) proc_nx += rx;
            if (proc_y == py - 1 && ry != 0) proc_ny += ry;
            
            recvcounts[proc] = proc_nx * proc_ny;
            if (proc == 0) {
                displs[proc] = 0;
            } else {
                displs[proc] = displs[proc-1] + recvcounts[proc-1];
            }
        }
    }

    MPI_Gatherv(local_data.data(), nx * ny, MPI_DOUBLE,
                global_data.data(), recvcounts.data(), displs.data(), MPI_DOUBLE,
                0, MPI_COMM_WORLD);

    // Write data (only processor 0)
    if (id == 0) {
        stringstream data_stream;
        
        // Write data in VTK order (j varies fastest, then i)
        for (int global_j = 0; global_j < py * ny; ++global_j) {
            for (int global_i = 0; global_i < px * nx; ++global_i) {
                // Determine which processor owns this point
                int proc_x = global_i / nx;
                int proc_y = global_j / ny;
                int proc_id = proc_y * px + proc_x;
                
                // Local indices within the processor
                int local_i = global_i % nx;
                int local_j = global_j % ny;
                
                // Find the data in the gathered array
                int data_idx = displs[proc_id] + local_j * nx + local_i;
                
                data_stream << fixed << setprecision(6) << global_data[data_idx];
                if (global_i == px * nx - 1) {
                    data_stream << "\n";
                } else {
                    data_stream << " ";
                }
            }
        }
        
        string data_str = data_stream.str();
        MPI_File_write_at(file, offsetH, data_str.c_str(), data_str.size(), MPI_CHAR, MPI_STATUS_IGNORE);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_File_close(&file);

    if (id == 0)
    {
        std::cout << "Parallel VTK file written: " << filename << std::endl;
    }
}

void Heat::writeCSV(int timeStep, int x, int y)
{

    std::string filename = "/../output/heat_diffusion" + to_string(timeStep) + ".csv";
    // std::string filename = "/mnt/c/Users/USUARIO/Documents/HPCPolimi/Year2/S1/NDSD/Project/Project3/output/heat_diffusion" + to_string(timeStep) + ".csv";

    if (id == 0)
    {
        std::remove(filename.c_str());
    }

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_File file;
    MPI_File_open(MPI_COMM_WORLD, filename.c_str(), MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &file);

    MPI_Offset offsetD = 0;
    MPI_Offset offsetH = 0;

    // writing in a csv file (x,y,temperature)

    // Write header
    stringstream ss;
    ss << "x,y,temperature\n";
    if (id == 0)
    {
        MPI_File_write_at(file, offsetH, ss.str().c_str(), ss.str().size(), MPI_CHAR, MPI_STATUS_IGNORE);
    }
    offsetH += ss.str().size();

    MPI_Barrier(MPI_COMM_WORLD);

    // Write data
    for (int i = 1; i < nx + 1; ++i)
    {
        for (int j = 1; j < ny + 1; ++j)
        {
            int precision = 9;
            int globalX = x * nx + (i - 1);
            int globalY = y * ny + (j - 1);

            stringstream ss, temp;
            temp << fixed << setprecision(precision) << grid[get(i,j)];
            ss << globalX << "," << globalY << "," << temp.str();

            string line = ss.str();
            line = line.substr(0, precision);
            line += "\n";

            // global lines written
            offsetD = offsetH + ((nx) * (ny) * (precision + 1)) * id ;
            // local lines already written
            offsetD += (j - 1) * (nx * (precision + 1)) + (i - 1) * (precision + 1);

            MPI_File_write_at(file, offsetD, line.c_str(), line.size(), MPI_CHAR, MPI_STATUS_IGNORE);

        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_File_close(&file);
}

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
        grid.resize((nx + 2) * (ny + 2));
        newGrid.resize((nx + 2) * (ny + 2));

    }
    if (rx != 0 && x == px - 1)
    {
        nx = nx + rx;
        grid.resize((nx + 2) * (ny + 2));
        newGrid.resize((nx + 2) * (ny + 2));
    }
    else if (ry != 0 && y == py - 1)
    {
        ny = ny + ry;
        grid.resize((nx + 2) * (ny + 2));
        newGrid.resize((nx + 2) * (ny + 2));
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
    initialCondition(100.0, x, y);

    // if (id == 1)
    // {
    //     cout << "Processor " << id << " after initialization: " << endl;
    //     printGrid();
    // }
    // cout << "Processor " << id << " before exchange: " << endl;
    // printGrid();

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
        // Exchange data between processors
        // idea: substitute for MPI_Sendrecv

        // Columns -------------------------

        //  Send data to the left
        if (neighbors[0] != -1)
        {
            MPI_Send(&grid[get(1,1)], 1, columnType, neighbors[0], id, MPI_COMM_WORLD);
        }
        // Receive data from the right
        if (neighbors[2] != -1)
        {
            MPI_Recv(&grid[get(1,nx + 1)], 1, columnType, neighbors[2], neighbors[2], MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        // Send data to the right
        if (neighbors[2] != -1)
        {
            MPI_Send(&grid[get(1,nx)], 1, columnType, neighbors[2], id, MPI_COMM_WORLD);
        }
        // Receive data from the left
        if (neighbors[0] != -1)
        {
            MPI_Recv(&grid[get(1,0)], 1, columnType, neighbors[0], neighbors[0], MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        // Rows -------------------------

        // Send data to the bottom
        if (neighbors[3] != -1)
        {
            MPI_Send(&grid[get(ny,1)], nx , MPI_DOUBLE, neighbors[3], id, MPI_COMM_WORLD);
        }
        // Receive data from the top
        if (neighbors[1] != -1)
        {
            MPI_Recv(&grid[get(0,1)], nx, MPI_DOUBLE, neighbors[1], neighbors[1], MPI_COMM_WORLD, MPI_STATUS_IGNORE );
        }

        // Send data to the top
        if (neighbors[1] != -1)
        {
            MPI_Send(&grid[get(1,1)], nx, MPI_DOUBLE, neighbors[1], id, MPI_COMM_WORLD);
        }
        // Receive data from the bottom
        if (neighbors[3] != -1)
        {
            MPI_Recv(&grid[get(ny + 1,1)], nx, MPI_DOUBLE, neighbors[3], neighbors[3], MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        MPI_Barrier(MPI_COMM_WORLD);

        // if (id == 0)
        // {
        //     cout << "Processor " << id << " after exchange: " << endl;
        //     printGrid();
        // }

        // cout << "Processor " << id << " after exchange: " << endl;
        // printGrid();

        // Apply boundary conditions
        //applyBoundaryConditions(10.0, x, y);

        // Compute new temperature values
        
        double maxChange = 0.0;
        for (int i = 1; i < nx + 2; ++i)
        {
            for (int j = 1; j < ny + 2; ++j)
            {
                newGrid[get(i,j)] = grid[get(i,j)] + alpha_dt_dx2 * (grid[get(i - 1,j)] + grid[get(i + 1,j)] + grid[get(i,j - 1)] + grid[get(i,j + 1)] - 4 * grid[get(i,j)]);

                maxChange = max(maxChange, abs(newGrid[get(i,j)] - grid[get(i,j)]));
            }
        }

        // Check for convergence

        double globalMaxChange;
        MPI_Allreduce(&maxChange, &globalMaxChange, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
        if (globalMaxChange < threshold)
        {
            cout << "Convergence achieved in " << step << " steps." << endl;
            writeCSV(step, x, y);
            break;
        }

        if (id == 0)
            // cout << "Step: " << step << ", Max Change: " << globalMaxChange << endl;
            cout << "Step: " << step << endl;

        writeCSV(step, x, y);

        // writeVTKParallel(step, x, y);
        
        // Swap grids
        grid.swap(newGrid);
    }

    MPI_Type_free(&columnType);

    // if (id == 0){
    //     writeVTK("heat_diffusion.vtk");
    // }

}
