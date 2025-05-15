### **Pipeline Architecture**

1.  **Kafka Producer** → Injects raw data into Kafka topics.
2.  **Akka Actor (Operator 1 - Aggregation Window A)** → Consumes messages, aggregates within a time window, and forwards results.
3.  **Kafka Topic (Intermediate Stage A)** → Stores results temporarily.
4.  **Akka Actor (Operator 2 - Aggregation Window B)** → Further processes results.
5.  **Kafka Topic (Final Output Stage)** → Stores final output.
6.  **Kafka Consumer** → Consumes results for external systems.

### Questions

1.  What determines the aggregation?
2.  The flushing messages are random?
3.  When an operator forwards the value with another key. What does it determines the forwrding and it still computes the aggregation and then forwards the value ?

### TODO

[X] Make Actor state fault tolerant
[X] Make actor supervisor
[ ] Fault injection
[ ] Customize windowing
[X] Use Kafka partitioning
[X] Use akka clustering
[ ] Test distributed

### Run locally

```
# Start Zookeeper and Kafka
docker compose up 
# Start producer
mvn exec:java -Pproducer
# Start consumer
mvn exec:java -Pconsumer
# Start output
mvn exec:java -Poutput

```