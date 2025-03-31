package com.example;

import akka.actor.*;
import akka.kafka.*;
import akka.kafka.javadsl.Consumer;
import akka.stream.*;
import akka.stream.javadsl.*;

import org.apache.kafka.clients.consumer.ConsumerConfig;
import org.apache.kafka.clients.consumer.ConsumerRecord;
import org.apache.kafka.clients.consumer.ConsumerRecords;
import org.apache.kafka.clients.consumer.KafkaConsumer;
import org.apache.kafka.common.serialization.StringDeserializer;

import java.util.Properties;
import java.util.Collections;
import java.time.Duration;
import java.time.temporal.ChronoUnit;
import java.io.IOException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import org.apache.kafka.clients.consumer.KafkaConsumer;

public class TemperatureConsumer {

    private static final String BOOTSTRAP_SERVERS = "localhost:9092";
    private static final String TOPIC = "temperature-data";
    private static final String GROUP_ID = "temperature-consumer-group";
    private static final String OFFSET_RESET_STRATEGY = "earliest"; // or "latest"
    private static final int NUM_THREADS = 10;

    public static void main(String[] args) {
        
        // Akka System
        final ActorSystem system = ActorSystem.create("TemperatureConsumerSystem");
        
        // Consumer settings
        final Properties consumerSettings = new Properties();
        consumerSettings.put(ConsumerConfig.BOOTSTRAP_SERVERS_CONFIG, BOOTSTRAP_SERVERS);
        consumerSettings.put(ConsumerConfig.GROUP_ID_CONFIG, GROUP_ID);
        consumerSettings.put(ConsumerConfig.KEY_DESERIALIZER_CLASS_CONFIG, StringDeserializer.class.getName());
        consumerSettings.put(ConsumerConfig.VALUE_DESERIALIZER_CLASS_CONFIG, StringDeserializer.class.getName());
        consumerSettings.put(ConsumerConfig.AUTO_OFFSET_RESET_CONFIG, OFFSET_RESET_STRATEGY);
        consumerSettings.put(ConsumerConfig.ISOLATION_LEVEL_CONFIG, "read_commited" );
        
        // Instantiating the consumer
        KafkaConsumer<String, String> consumer = new KafkaConsumer<>(consumerSettings);
        consumer.subscribe(Collections.singletonList(TOPIC));

        // Creating an Actor for processing messages
        final ActorRef aggActor = system.actorOf(TAggragationActor.props(10, null, "output", "average", "output"), "aggActor");

        // Executor Service to submit tasks
        final ExecutorService executorService = Executors.newFixedThreadPool(NUM_THREADS);

        while (true) {
            // Polling for new records
            final ConsumerRecords<String, String> records = consumer.poll(Duration.ofMillis(1000));
            for (ConsumerRecord<String, String> record : records) {
                System.out.printf("Received message: key=%s, value=%s, partition=%d, offset=%d%n",
                        record.key(), record.value(), record.partition(), record.offset());
                        // Submit the processing task to the executor service
                        try{
                            double value = Double.parseDouble(record.value());
                            executorService.submit(() -> {
                                aggActor.tell(value, ActorRef.noSender());
                            });
                        } catch (NumberFormatException e) {
                            System.err.println("Invalid number format: " + record.value());
                        } 
            }

        }
        
    }
    
}
