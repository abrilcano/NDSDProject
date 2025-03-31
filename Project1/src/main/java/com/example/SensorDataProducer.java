package com.example;

import java.util.Properties;
import java.util.Random;
import org.apache.kafka.clients.producer.*;
import org.apache.kafka.common.serialization.StringSerializer;


public class SensorDataProducer {
    private static final String BOOTSTRAP_SERVERS = "localhost:9092";
    private static final String TRANSACTIONAL_ID = "sensor-producer-transactional";
    private static final String[] SENSOR_TYPES = {"temperature", "humidity", "wind", "airQuality"};
    
    private static final Random random = new Random();
    
    public static void main(String[] args) {

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
                    String topic = sensor + "-data";
                    String value = generateSensorReading(sensor);
                    ProducerRecord<String, String> record = new ProducerRecord<>(topic, sensor, value);
                    producer.send(record);
                    System.out.printf("Produced -> Topic: %s, Key: %s, Value: %s\n", topic, sensor, value);
                }
                
                producer.commitTransaction();
                Thread.sleep(10000); // Simulate sensor reading every second
            }
        } catch (Exception e) {
            producer.abortTransaction();
            e.printStackTrace();
        } finally {
            producer.close();
        }
    }
    
    private static String generateSensorReading(String sensorType) {
        return switch (sensorType) {
            case "temperature" -> String.format("%.1f", 15 + random.nextDouble() * 10);
            case "humidity" -> String.format("%d", 30 + random.nextInt(50));
            case "wind" -> String.format("%d ", 5 + random.nextInt(20));
            case "airQuality" -> String.format("%d ", 10 + random.nextInt(200));
            default -> "unknown";
        };
    }
}
