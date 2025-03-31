### **Pipeline Architecture**

1. **Kafka Producer** → Injects raw data into Kafka topics.
2. **Akka Actor (Operator 1 - Aggregation Window A)** → Consumes messages, aggregates within a time window, and forwards results.
3. **Kafka Topic (Intermediate Stage A)** → Stores results temporarily.
4. **Akka Actor (Operator 2 - Aggregation Window B)** → Further processes results.
5. **Kafka Topic (Final Output Stage)** → Stores final output.
6. **Kafka Consumer** → Consumes results for external systems.
