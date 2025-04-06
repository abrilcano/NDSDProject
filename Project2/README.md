### **Pipeline Architecture**

1. **Kafka Producer** → Injects raw data into Kafka topics.
2. **Akka Actor (Operator 1 - Aggregation Window A)** → Consumes messages, aggregates within a time window, and forwards results.
3. **Kafka Topic (Intermediate Stage A)** → Stores results temporarily.
4. **Akka Actor (Operator 2 - Aggregation Window B)** → Further processes results.
5. **Kafka Topic (Final Output Stage)** → Stores final output.
6. **Kafka Consumer** → Consumes results for external systems.

### Questions

1. What determines the aggregation?
2. The flushing messages are random?
3. When an operator forwards the value with another key. ! what does it determines the forwrding and it still computes the aggregation and then forwards the value ?

### TODO

* [ ] Make Actor state fault tolerant
* [ ] Make actor supervisor
* [ ] Fault injection
* [ ] Replicate for other actors
* [ ] Use Kafka partitioning
* [ ] Use akka clustering
* [ ] Make distributed
