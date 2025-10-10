docker compose up 
Project #2 · Stream Processing System
=====================================

Fault-tolerant Akka + Kafka pipeline that ingests sensor readings, aggregates them over sliding windows, and forwards the results to downstream topics. Each logical operator runs as a sharded Akka actor so that keys are partitioned deterministically across cluster members. Kafka transactions and Akka persistence guarantee exactly-once semantics even in the presence of process failures.

Project Layout
--------------
- `src/main/java/com/polimi/producer` – Kafka producer that synthesises sensor readings and writes to the `sensor-data` topic with idempotent transactions.
- `src/main/java/com/polimi/consumers` – Consumers that feed the Akka pipeline (`SensorDataConsumer`) and print aggregated results (`OutputConsumer`).
- `src/main/java/com/polimi/actors` – Actor hierarchy (`SensorActorSupervisor` + sensor-specific actors) implementing the windowing logic and forwarding to the next Kafka stage.
- `src/main/java/com/polimi/utils` – Message envelopes and window snapshots used for persistence.
- `src/main/java/com/polimi/fault` – `FaultInjector` helpers to trigger controlled crashes for resiliency testing.
- `src/main/resources/application.conf` – Akka Cluster, persistence, and remoting configuration.

Processing Model
----------------
- **Input** – Raw sensor events (`<key=sensorType, value=reading>`), produced to Kafka topic `sensor-data` with four partitions.
- **Shard routing** – `SensorDataConsumer` uses Akka Cluster Sharding to dispatch each event to a sensor-specific actor instance based on the key hash so all values for a given sensor land on the same actor.
- **Windowing** – Each actor maintains an in-memory sliding window (size + slide configurable per sensor type) and persists events via Akka Persistence for crash recovery.
- **Aggregations** – Supported functions include `average`, `sum`, `min`, and `max`. Results are sent transactionally to the next Kafka topic (`output` by default, see actor implementation for overrides).
- **Reset signal** – A `FlushCommand` can be sent at any time (manually or through anomaly detection heuristics) to reset the actor state to an empty window.
- **Exactly once** – The pipeline combines Kafka read-committed consumers, transactional producers, and persistent actors to ensure each `<key,value>` is processed exactly once even when processes crash.

Prerequisites
-------------
- Java 14 (configurable via `maven.compiler.source/target` in `pom.xml`).
- Maven 3.8+.
- Docker Compose (for local Kafka/ZooKeeper) or access to a remote Kafka cluster.
- `docker compose` file providing Kafka brokers (place it at the project root if you plan local runs).
- Network connectivity between the producer node and the consumer/actor node (defaults use different broker addresses for each machine).

Configuration
-------------
1. **Kafka brokers** – Update the `BOOTSTRAP_SERVERS` constant in:
	- `SensorDataProducer` (default `100.90.45.66:9092`).
	- `SensorDataConsumer` and `OutputConsumer` (default `172.20.10.1:9092`).
	Ensure both point to the correct broker(s) for their deployment environment.
2. **Window sizes** – Adjust per-sensor window size/slide pairs in `SensorActorSupervisor`'s constructor.
3. **Actor remoting** – Review `application.conf` to change cluster seeds, bind addresses, or persistence directories. Set `akka.remote.artery.canonical.hostname` to the public IP of the consumer host when running across machines.
4. **Kafka topics** – Producer auto-creates `sensor-data`. Additional downstream topics (e.g. `temperature-data`) are created on demand by the consumer when rerouting messages; pre-create them if your broker restricts auto-creation.
5. **Fault injection** – Call `FaultInjector` helpers from actors or consumers to simulate failures. Existing code uses heuristic resets via `shouldTriggerReset` in `SensorDataConsumer`.

Running Locally
---------------
1. Start Kafka and ZooKeeper (or Kraft) locally:

	```powershell
	docker compose up
	```

2. Launch the producer (optionally on a separate machine closer to the sensors):

	```powershell
	mvn exec:java -Pproducer
	```

3. Launch the Akka consumer cluster member. For multi-node clusters, repeat on each node with distinct `application.conf` ports:

	```powershell
	mvn exec:java -Pconsumer
	```

4. Launch the output sink to observe aggregated results:

	```powershell
	mvn exec:java -Poutput
	```


Distributed Deployment
----------------------
- Deploy the producer on Machine A (close to the sensors) pointing at the broker reachable from both machines.
- Deploy the consumers/actors on Machine B with Akka cluster enabled; configure `seed-nodes` in `application.conf` to reference Machine B (and peers if clustering).
- Forward the aggregated Kafka topic (`output`) either to analysts or additional downstream services. The sample `OutputConsumer` simply prints the records but can be replaced with a real sink.

Observability & Fault Tolerance
--------------------------------
- Actor crashes automatically restart under the supervisor strategy; state is restored from the latest persisted snapshot.
- Kafka transactional producers guarantee either commit or abort for each batch, ensuring no duplicate aggregates reach the output topic.
- Use Kafka consumer lag monitoring to ensure partitions remain balanced across cluster members; the sharding extractor uses modulus on the sensor type hash to keep routing deterministic.
