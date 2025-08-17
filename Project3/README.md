### Description

### Compilation

mpicxx main.cpp heat.cpp -o heat
mvn clean package

### Execution
mpirun -np n ./heat

### Bibliography

https://people.math.sc.edu/burkardt/cpp_src/heat_mpi/heat_mpi.html

https://people.sc.fsu.edu/~jburkardt/cpp_src/fd2d_heat_steady/fd2d_heat_steady.html

https://github.com/cschpc/heat-equation/blob/main/mpi/main.cpp

https://www-udc.ig.utexas.edu/external/becker/teaching/557/problem_sets/problem_set_fd_2dheat.pdf

https://www.paulnorvig.com/guides/using-mpi-with-c.html

https://rantahar.github.io/introduction-to-mpi/setup.html

https://people.math.sc.edu/Burkardt/cpp_src/mpi/mpi.html

### TODO 
- [ ] Implement MPI_reduce to check convergence in all processes 

