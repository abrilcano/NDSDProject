#ifndef HEAT_HPP
#define HEAT_HPP

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>

class Heat
{
public:
  // Constructor
  Heat(double alpha, double dt, double dx, double L, double threshold, int maxSteps, int p, int id) 
      : alpha(alpha), dt(dt), dx(dx), L(L), threshold(threshold), maxSteps(maxSteps),
        n(static_cast<int>(L / dx)),
        p(p),
        id(id),
        px(floor(sqrt(p))),
        py(p / px),
        nx(floor(n / px)),
        ny(floor(n / py)),
        rx(n % px),
        ry(n % py),
        grid((nx + 2) * (ny + 2), -1.0),
        newGrid((nx + 2) * (ny + 2), -1.0)
        {}

  void initializeGhostValues();
  void initialCondition(double cornerTemp, int x, int y);
  void printGrid();
  int get(int i, int j);
  void writeTXT(int timeStep, int x, int y);
  void writeVTK(const std::string &filename);
  void applyBoundaryConditions(double cornerTemp, int x, int y);
  void solve();

protected:
  double alpha;     // Thermal diffusivity constant
  double dt;        // Time step
  double dx;        // Spatial step
  double L;         // Side length of the grid
  double threshold; // Convergence threshold
  int maxSteps;     // Maximum number of steps
  int n;            // Number of grid points
  int p;            // Number of processors
  int id;           // Processor id
  // Processors working in each direction
  int px;
  int py;
  // Number of grid points in each direction
  int nx;
  int ny;
  // Remainder grid points
  int rx = n % px;
  int ry = n % py;
  // grid
  std::vector<double> newGrid;
  std::vector<double> grid;

};

#endif