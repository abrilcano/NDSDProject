# Project #3 · Heat Diffusion Simulation

Parallel pipeline that simulates 2D heat diffusion with MPI and then analyses the results with Apache Spark. The simulation discretises the heat equation on a regular grid, writes each timestep to disk, and Spark aggregates the numeric series to extract trends per grid point.

## Contents

*   `MPI/` – C++ implementation of an MPI stencil solver (`Heat.cpp`, `Heat.hpp`, `main.cpp`).
*   `output/` – CSV snapshots produced by the MPI job, one file per timestep.
*   `results/` – Spark query outputs pulled back to the local machine.
*   `SparkAnalyzer/` – Maven project with the Spark jobs (compiled remotely by the orchestrator).
*   `orchestrator.py` – Python script that compiles, runs, and coordinates MPI + Spark across machines.

## Simulation Model

*   The solver integrates the heat equation `∂T/∂t = α (∂²T/∂x² + ∂²T/∂y²)` using a five-point finite-difference stencil.
*   At each timestep `t + Δt` the temperature of cell `(x, y)` is updated from its four neighbours and stored to disk.
*   Convergence occurs when the absolute temperature change of every grid point is below a threshold or when the maximum step budget is exhausted.
*   Default parameters (tunable in `MPI/main.cpp` via the `Heat` constructor):
    *   `alpha` – thermal diffusivity constant (default `0.01`).
    *   `dt` – temporal step in seconds (default `0.001`).
    *   `dx` – spatial discretisation step (default `1`).
    *   `L` – grid side length (default `100` points).
    *   `threshold` – convergence tolerance (default `1e-5`).
    *   `maxSteps` – maximum iterations before early stop (default `300`).

## Spark Analytics

The Spark application ingests all CSV snapshots and produces three derived datasets, one row per grid cell:

1.  Minimum, maximum, and average temperature difference over the full simulation.
2.  Sliding-window temperature variation (window size `100·Δt`, slide `10·Δt`).
3.  Maximum windowed difference (max over the values computed in step 2).

## Prerequisites

*   Python 3.8+ on the orchestration host (for `orchestrator.py`).
*   WSL with an MPI toolchain accessible via `mpicxx`/`mpirun` (the orchestrator uses `wsl bash -c ...`).
*   Password-less SSH between the orchestration host and the Spark processor (`vboxuser@<processor-ip>` in the defaults).
*   rsync installed on both machines.
*   Remote machine prepared with:
    *   Apache Spark (Standalone mode) installed under `/opt/spark`.
    *   Java + Maven to build `SparkAnalyzer`.
    *   A clone or copy of the project under `/home/vboxuser/Documents/`.

## Configuration

1.  **Network** – set `processor_ip` and `local_ip` in `orchestrator.py` to match your Spark node and orchestrator host.
2.  **MPI parameters** – edit the `Heat` constructor arguments inside `MPI/main.cpp` to adjust physical constants, grid size, or convergence criteria. Recompile is handled by the orchestrator.
3.  **Spark executor** – confirm Spark master/worker scripts (`start-master.sh`, `start-worker.sh`) live under `/opt/spark/sbin/`. Adjust the commands in `orchestrator.py` if your installation differs.
4.  **Remote paths** – the orchestrator expects the remote directories:
    *   `/home/vboxuser/Documents/SparkAnalyzer` for the analytics project.
    *   `/home/vboxuser/Documents/results` to receive query outputs.  
        Update the rsync/ssh paths if your layout is different.

## Running the Pipeline

Ensure both machines are reachable and SSH keys are configured.

From the project root, execute the orchestrator:

The script performs the following steps automatically:

*   Compiles and runs the MPI solver under WSL, producing CSV snapshots in `output/`.
*   Transfers snapshots to the Spark node via rsync.
*   Starts Spark master and worker, builds the SparkAnalyzer JAR with Maven, and runs the Spark job.
*   Stops the Spark daemons and rsyncs the aggregated results back into `results/` (subfolders `originalData`, `Q1`, `Q2`, `Q3`).
