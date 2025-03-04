#ifndef HEAT_HPP
#define HEAT_HPP

#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <cmath>

class Heat
{
public:
  // Constructor
  Heat(double alpha, double dt, double dx, double L, double threshold, int maxSteps, int p, int id) 
      : alpha(alpha), dt(dt), dx(dx), L(L), threshold(threshold), maxSteps(maxSteps),
        n(static_cast<int>(L / dx)),
        p(p),
        // py(floor(sqrt(p))),
        // px(p / floor(sqrt(p))), 
        px(floor(sqrt(p))),
        py(p / px),
        nx(floor(n / px)),
        ny(floor(n / py)),
        id(id),
        rx(n % px),
        ry(n % py),
        // grid(ny + 2 , std::vector<double>(nx + 2, 1.0)),
        // newGrid(ny + 2, std::vector<double>(nx + 2, 1.0))
        grid(nx + 2 , std::vector<double>(ny + 2, - 1.0)),
        newGrid(nx + 2, std::vector<double>(ny + 2, - 1.0)),
        contiguosGrid((nx + 2) * (ny + 2), -1.0),
        contiguosNewGrid((nx + 2) * (ny + 2), -1.0)
        {}

  void initializeGrid(double initialTemp);
  void initializeGhostValues();
  void initialCondition(double cornerTemp, int x, int y);
  void printGrid();
  int get(int i, int j);
  void output(int timeStep);
  void applyBoundaryConditions();
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
  // Processors working in each direction
  int px;
  int py;
  // Number of grid points in each direction
  int nx;
  int ny;
  // Remainder grid points
  int rx = n % px;
  int ry = n % py;
  int id; // Processor id
  std::vector<std::vector<double>> grid;
  std::vector<std::vector<double>> newGrid;
  // contiguos single array
  std::vector<double> contiguosNewGrid;
  std::vector<double> contiguosGrid;

};

#endif