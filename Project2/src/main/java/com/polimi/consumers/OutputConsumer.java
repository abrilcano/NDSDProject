package com.polimi.consumers;

import org.apache.kafka.clients.consumer.*;
import org.apache.kafka.common.serialization.StringDeserializer;
import org.apache.kafka.common.serialization.StringSerializer;

import java.util.Properties;
import java.util.Collections;
import java.time.Duration;

public class OutputConsumer {
    // Consumer
    private static final String BOOTSTRAP_SERVERS = "172.20.10.1:9092";
    private static final String TOPIC = "output";
    private static final String GROUP_ID = "output-consumer-group";
    private static final String OFFSET_RESET_STRATEGY = "earliest"; // or "latest"

    public static void main(String[] args) {
        // Consumer settings
        final Properties consumerSettings = new Properties();
        consumerSettings.put(ConsumerConfig.BOOTSTRAP_SERVERS_CONFIG, BOOTSTRAP_SERVERS);
        consumerSettings.put(ConsumerConfig.GROUP_ID_CONFIG, GROUP_ID);
        consumerSettings.put(ConsumerConfig.KEY_DESERIALIZER_CLASS_CONFIG, StringDeserializer.class.getName());
        consumerSettings.put(ConsumerConfig.VALUE_DESERIALIZER_CLASS_CONFIG, StringDeserializer.class.getName());
        consumerSettings.put(ConsumerConfig.AUTO_OFFSET_RESET_CONFIG, OFFSET_RESET_STRATEGY);
        consumerSettings.put(ConsumerConfig.ISOLATION_LEVEL_CONFIG, "read_committed" );

        // Create a Kafka consumer
        try (KafkaConsumer<String, String> consumer = new KafkaConsumer<>(consumerSettings)) {
            consumer.subscribe(Collections.singletonList(TOPIC));
            
            while (true) {
                ConsumerRecords<String, String> records = consumer.poll(Duration.ofMillis(1000));
                for (ConsumerRecord<String, String> record : records) {
                    System.out.printf("Consumed -> Topic: %s, Key: %s, Value: %s\n", record.topic(), record.key(), record.value());
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
