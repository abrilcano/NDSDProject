package com.polimi.producer;

import java.util.Locale;
import java.util.Properties;
import java.util.Random;
import java.util.Collections;
import java.util.concurrent.ExecutionException;

import org.apache.kafka.clients.admin.AdminClient;
import org.apache.kafka.clients.admin.AdminClientConfig;
import org.apache.kafka.clients.admin.CreateTopicsResult;
import org.apache.kafka.clients.admin.NewTopic;
import org.apache.kafka.clients.producer.*;
import org.apache.kafka.common.KafkaFuture;
import org.apache.kafka.common.serialization.StringSerializer;


public class SensorDataProducer {
    private static final String BOOTSTRAP_SERVERS = "localhost:9092";
    private static final String TRANSACTIONAL_ID = "sensor-producer-transactional";
    private static final String[] SENSOR_TYPES = {"temperature", "humidity", "wind", "airQuality"};

    private static final int NUM_PARTITIONS = 4;
    private static final short NUM_REPLICAS = 1;
    private static final String TOPIC_NAME = "sensor-data";
    
    private static final Random random = new Random();
    
    public static void main(String[] args) {

        // Create the topic if it doesn't exist
        createTopic(TOPIC_NAME, NUM_PARTITIONS, NUM_REPLICAS);

        Properties props = new Properties();
        props.put(ProducerConfig.BOOTSTRAP_SERVERS_CONFIG, BOOTSTRAP_SERVERS);
        props.put(ProducerConfig.TRANSACTIONAL_ID_CONFIG, TRANSACTIONAL_ID);
        props.put(ProducerConfig.KEY_SERIALIZER_CLASS_CONFIG, StringSerializer.class.getName());
        props.put(ProducerConfig.VALUE_SERIALIZER_CLASS_CONFIG, StringSerializer.class.getName());
        props.put(ProducerConfig.ENABLE_IDEMPOTENCE_CONFIG, "true");
        props.put(ProducerConfig.ACKS_CONFIG, "all");
        props.put(ProducerConfig.RETRIES_CONFIG, Integer.MAX_VALUE);
        
        KafkaProducer<String, String> producer = new KafkaProducer<>(props);
        producer.initTransactions();
        
        try {
            while (true) {
                producer.beginTransaction();
                
                for (String sensor : SENSOR_TYPES) {
                    String topic = "sensor-data";
                    String value = generateSensorReading(sensor);
                    ProducerRecord<String, String> record = new ProducerRecord<>(topic, sensor, value);
                    producer.send(record);
                    Thread.sleep(2500);
                    System.out.printf("Produced -> Topic: %s, Key: %s, Value: %s\n", topic, sensor, value);
                }
                
                producer.commitTransaction();
                Thread.sleep(25000); // Simulate sensor reading every second
            }
        } catch (Exception e) {
            producer.abortTransaction();
            e.printStackTrace();
        } finally {
            producer.close();
        }
    }

    private static void createTopic(String topicName, int numPartitions, short replicationFactor) {
        Properties props = new Properties();
        props.put(AdminClientConfig.BOOTSTRAP_SERVERS_CONFIG, BOOTSTRAP_SERVERS);
        
        try (AdminClient adminClient = AdminClient.create(props)) {
            // Check if topic exists
            if (!adminClient.listTopics().names().get().contains(topicName)) {
                NewTopic newTopic = new NewTopic(topicName, numPartitions, replicationFactor);
                CreateTopicsResult result = adminClient.createTopics(Collections.singleton(newTopic));
                
                // Wait for the operation to complete
                KafkaFuture<Void> future = result.values().get(topicName);
                future.get();
                
                System.out.println("Topic created: " + topicName + 
                                  " with " + numPartitions + " partitions and " +
                                  replicationFactor + " replicas");
            } else {
                System.out.println("Topic already exists: " + topicName);
            }
        } catch (InterruptedException | ExecutionException e) {
            System.err.println("Error creating topic: " + e.getMessage());
        }
    }

    
    private static String generateSensorReading(String sensorType) {
        return switch (sensorType) {
            case "temperature" -> String.format(Locale.US, "%.1f", 15 + random.nextDouble() * 10);
            case "humidity" -> String.format("%d", 30 + random.nextInt(50));
            case "wind" -> String.format("%d", 5 + random.nextInt(20));
            case "airQuality" -> String.format("%d", 10 + random.nextInt(200));
            default -> "unknown";
        };
    }
}
