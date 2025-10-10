Networked Software for Distributed Systems
======================

Repository of three course projects covering wireless IoT routing, fault-tolerant stream processing, and parallel heat simulation. Every subfolder ships with a detailed README; use this overview to orient yourself before diving into the project-specific documentation.

Technology Recap
----------------
- **Project1**: Contiki-NG, COOJA simulator, MQTT, & Node-RED.
- **Project2**: Apache Kafka & Akka Cluster & Persistence.
- **Project3**: MPI, Apache Spark.

Project Summary
---------------
- `Project1` – **RPL Routing Monitor**. Contiki-NG firmware for simulated motes publishes RPL routing statistics to MQTT; a Node-RED backend reconstructs the wireless topology and compares OF0 vs MRHOF objective functions.
- `Project2` – **Stream Processing System**. Akka Cluster actors backed by Kafka topics deliver exactly-once sliding-window aggregations over sensor data, with transactional producers, sharded consumers, and failure injection utilities.
- `Project3` – **Heat Diffusion Simulation**. MPI solver integrates the 2D heat equation and streams snapshots to Spark, which computes temporal analytics; a Python orchestrator coordinates multi-host execution.

Repository Layout
-----------------
```
Project/
├── Project1/    # Contiki-NG MQTT client, Node-RED flow, RPL analysis docs
├── Project2/    # Java/Akka/Kafka streaming pipeline with Maven build
├── Project3/    # MPI + Spark hybrid workflow with Python orchestrator
└── NSDS Projects 2024.pdf  # Assignment brief
```

Prerequisites at a Glance
-------------------------
- **Project1**: Contiki-NG toolchain (Cooja or supported hardware), MQTT broker, Node-RED with FlowFuse dashboard.
- **Project2**: Java 14, Maven 3.8+, Docker Compose or external Kafka cluster, network access between producer and consumer hosts.
- **Project3**: Python 3.8+, WSL with MPI toolchain, rsync/SSH access to remote Spark node, Apache Spark + Maven on the analytics host.


