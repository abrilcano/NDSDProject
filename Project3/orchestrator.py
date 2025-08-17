import subprocess

def run_step(verbose, description, command, shell):
    if verbose:
        print(f"Running step: {description}")
    result = subprocess.run(["wsl", "bash", "-c", command], shell=shell, check=True, text=True, capture_output=True)
    print(result.stdout)
    if result.returncode != 0:
        raise RuntimeError(f"Command failed: {command}")
        exit(1)
    if verbose:
        print(f"Step completed: {description}")
    
if __name__ == "__main__":

    processor_ip = "192.168.1.158"
    local_ip = "192.168.1.80"

    # Run MPI Simulation
    run_step(True, "MPI compile", "mpicxx MPI/main.cpp MPI/heat.cpp -o MPI/main ", shell=True)
    run_step(True, "MPI Simulation", "mpirun -np 4 MPI/main", shell=True)
    # Send result files to Processor
    run_step(True, "Send result files to Processor", "rsync -avz -e ssh /mnt/c/Users/USUARIO/Documents/HPCPolimi/Year2/S1/NDSD/Project/Project3/output vboxuser@" + processor_ip + ":/home/vboxuser/Documents/", shell=True)
    # Connect to Processor and run Spark Analysis
    remote_commands = (
        "cd /home/vboxuser/Documents/SparkAnalyzer && "
        "/opt/spark/bin/spark-submit --class polimi.spark.HeatDiffusionAnalysis target/SparkAnalyzer-1.0.jar spark://" + processor_ip + ":7077"
    )
    # run_step(True, "Start Master on remote processor", "ssh vboxuser@" + processor_ip + " '/opt/spark/sbin/start-master.sh --host " + processor_ip + "'", shell=True)
    # run_step(True, "Start Worker on remote processor", "ssh vboxuser@" + processor_ip + " '/opt/spark/sbin/start-worker.sh spark://" + processor_ip + ":7077'", shell=True)
    # run_step(True, "Compile Spark Application on remote processor", "ssh vboxuser@" + processor_ip + " 'cd /home/vboxuser/Documents/SparkAnalyzer && mvn clean package'", shell=True)
    run_step(True, "Run Spark Analysis on remote processor", "ssh vboxuser@" + processor_ip + " '" + remote_commands + "'", shell=True)
    # run_step(True, "Stop worker on remote processor", "ssh vboxuser@" + processor_ip + " '/opt/spark/sbin/stop-worker.sh" + "'", shell=True)
    # run_step(True, "Stop master on remote processor", "ssh vboxuser@" + processor_ip + " '/opt/spark/sbin/stop-master.sh" + "'", shell=True)
    # Pull back results from processor
    run_step(False, "Send result files back to local machine", "rsync -avz -e ssh vboxuser@" + processor_ip + ":/home/vboxuser/Documents/results/original_data.csv/part* /mnt/c/Users/USUARIO/Documents/HPCPolimi/Year2/S1/NDSD/Project/Project3/results/originalData", shell=True)
    run_step(False, "Send result files back to local machine", "rsync -avz -e ssh vboxuser@" + processor_ip + ":/home/vboxuser/Documents/results/query1_results.csv/part* /mnt/c/Users/USUARIO/Documents/HPCPolimi/Year2/S1/NDSD/Project/Project3/results/Q1", shell=True)
    run_step(False, "Send result files back to local machine", "rsync -avz -e ssh vboxuser@" + processor_ip + ":/home/vboxuser/Documents/results/query2_results.csv/part* /mnt/c/Users/USUARIO/Documents/HPCPolimi/Year2/S1/NDSD/Project/Project3/results/Q2", shell=True)
    run_step(True, "Pull result files back to local machine", "rsync -avz -e ssh vboxuser@" + processor_ip + ":/home/vboxuser/Documents/results/query3_results.csv/part* /mnt/c/Users/USUARIO/Documents/HPCPolimi/Year2/S1/NDSD/Project/Project3/results/Q3", shell=True)
    print("All steps completed successfully.")


