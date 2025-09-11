package com.polimi.consumers;

import com.polimi.utils.DataMessage;
import com.polimi.utils.FlushCommand;
import com.polimi.actors.SensorActorSupervisor;

import akka.actor.*;
import akka.cluster.sharding.ShardRegion;
import akka.cluster.sharding.ClusterSharding;
import akka.cluster.sharding.ClusterShardingSettings;

import org.apache.kafka.clients.consumer.*;
import org.apache.kafka.clients.producer.KafkaProducer;
import org.apache.kafka.clients.producer.ProducerConfig;
import org.apache.kafka.clients.producer.ProducerRecord;
import org.apache.kafka.common.serialization.StringDeserializer;
import org.apache.kafka.common.serialization.StringSerializer;

import java.util.Properties;
import java.util.Collections;
import java.time.Duration;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class SensorDataConsumer {

    // Consumer
    private static final String BOOTSTRAP_SERVERS = "172.20.10.1:9092";
    private static final String TOPIC = "sensor-data";
    private static final String GROUP_ID = "sensor-consumer-group";
    private static final String OFFSET_RESET_STRATEGY = "earliest"; // or "latest"

    // Producer
    private static final String TRANSACTIONAL_ID = "sensor-consumer-transactional";
    private static final String[] SENSOR_TYPES = {"temperature", "humidity", "wind", "airQuality"};

    // Actor System
    private static final int NUM_THREADS = 8;
    // Shutdown flag
    private static volatile boolean running = true;

    // Sharding message Extractor - Optimized for exactly-once semantics
    private static ShardRegion.MessageExtractor messageExtractor = new ShardRegion.MessageExtractor() {
        @Override
        public String entityId(Object message) {
            if (message instanceof DataMessage) {
                DataMessage msg = (DataMessage) message;
                return msg.getSensorType();
            } else if (message instanceof FlushCommand) {
                FlushCommand flushMsg = (FlushCommand) message;
                return flushMsg.getSensorType();
            }
            return null;
        }

        @Override
        public Object entityMessage(Object message) {
            return message;
        }

        @Override
        public String shardId(Object message) {
            if (message instanceof DataMessage) {
                String sensorType = ((DataMessage) message).getSensorType();
                return String.valueOf(Math.abs(sensorType.hashCode()) % 4);
            } else if (message instanceof FlushCommand) {
                String sensorType = ((FlushCommand) message).getSensorType();
                return String.valueOf(Math.abs(sensorType.hashCode()) % 4);
            } else {
                return null;
            }
        }
    };

    public static void main(String[] args) {

        // Akka System and shariding
        final ActorSystem system = ActorSystem.create("DataConsumerSystem");
        initializeSharding(system);

        ActorRef shardRegion = ClusterSharding.get(system).shardRegion("SensorActorSupervisor");

        // Consumer settings
        final Properties consumerSettings = new Properties();
        consumerSettings.put(ConsumerConfig.BOOTSTRAP_SERVERS_CONFIG, BOOTSTRAP_SERVERS);
        consumerSettings.put(ConsumerConfig.GROUP_ID_CONFIG, GROUP_ID);
        consumerSettings.put(ConsumerConfig.KEY_DESERIALIZER_CLASS_CONFIG, StringDeserializer.class.getName());
        consumerSettings.put(ConsumerConfig.VALUE_DESERIALIZER_CLASS_CONFIG, StringDeserializer.class.getName());
        consumerSettings.put(ConsumerConfig.AUTO_OFFSET_RESET_CONFIG, OFFSET_RESET_STRATEGY);
        consumerSettings.put(ConsumerConfig.ISOLATION_LEVEL_CONFIG, "read_committed");

        // Producer settings
        final Properties producerSettings = new Properties();
        producerSettings.put(ProducerConfig.BOOTSTRAP_SERVERS_CONFIG, BOOTSTRAP_SERVERS);
        producerSettings.put(ProducerConfig.KEY_SERIALIZER_CLASS_CONFIG, StringSerializer.class.getName());
        producerSettings.put(ProducerConfig.VALUE_SERIALIZER_CLASS_CONFIG, StringSerializer.class.getName());
        producerSettings.put(ProducerConfig.TRANSACTIONAL_ID_CONFIG, TRANSACTIONAL_ID);
        producerSettings.put(ProducerConfig.ENABLE_IDEMPOTENCE_CONFIG, "true");

        // Instantiating the consumer
        KafkaConsumer<String, String> consumer = new KafkaConsumer<>(consumerSettings);
        consumer.subscribe(Collections.singletonList(TOPIC));

        // Instantiating the producer
        KafkaProducer<String, String> producer = new KafkaProducer<>(producerSettings);
        producer.initTransactions();

        // Executor Service to submit tasks
        final ExecutorService executorService = Executors.newFixedThreadPool(NUM_THREADS);

        // Shutdown hook to handle exit
        Runtime.getRuntime().addShutdownHook(new Thread(() -> {
            System.out.println("Shutting down gracefully...");
            running = false; // Stop the polling loop
            consumer.wakeup(); // Interrupt Kafka polling safely
            executorService.shutdown(); // Shutdown the executor service
            producer.close(); // Close the producer to prevent resource leak
            system.terminate(); // Terminate the Akka system
        }));

        try {
            while (running) {
                ConsumerRecords<String, String> records = consumer.poll(Duration.ofMillis(1000));
                for (ConsumerRecord<String, String> record : records) {
                    System.out.printf("Consumer received message: key=%s, value=%s, offset=%d%n",
                            record.key(), record.value(), record.offset());

                    try {
                        // Create a DataMessage object from the record value
                        DataMessage dataMsg = new DataMessage(Double.parseDouble(record.value()), "average", record.key());

                        // Send the sensor value to the aggregation actor asynchronously
                        executorService.submit(() -> shardRegion.tell(dataMsg, ActorRef.noSender()));

                        // Example: Reset logic based on data anomalies
                        if (shouldTriggerReset(record.value(), record.key())) {
                            FlushCommand flushCmd = new FlushCommand(record.key(), "Data anomaly detected");
                            executorService.submit(() -> shardRegion.tell(flushCmd, ActorRef.noSender()));
                            System.out.printf("Flush signal sent for sensor type: %s\n", record.key());
                        }

                        // Randomized forwarding to other topics with a probability of 0.1
                        if (Math.random() < 0.1) {

                            try {
                                producer.beginTransaction();

                                // Randomly select a sensor type and forward the message to the corresponding topic
                                String randomSensorType = SENSOR_TYPES[(int) (Math.random() * SENSOR_TYPES.length)];
                                String randomTopic = randomSensorType + "-data";
                                ProducerRecord<String, String> producerRecord = new ProducerRecord<>(randomTopic, randomSensorType, record.value());
                                System.out.printf("Forwarding message to topic %s: %s\n", randomTopic, record.value());

                                producer.send(producerRecord).get();
                                producer.commitTransaction();

                            } catch (Exception e) {
                                System.err.println("Error in transaction: " + e.getMessage());
                                producer.abortTransaction();
                            }
                        }

                    } catch (Exception e) {
                        System.err.println("Error processing record: " + e.getMessage());
                        System.err.println("Invalid temperature value: " + record.value());
                    }
                }
            }
        } catch (Exception e) {
            System.err.println("Error in Kafka consumer: " + e.getMessage());
        } finally {
            consumer.close();
            executorService.shutdown();
            system.terminate();
            System.out.println("Consumer shutdown complete.");
        }

    }

    public static void initializeSharding(ActorSystem system) {
        ClusterSharding.get(system).start(
                "SensorActorSupervisor",
                SensorActorSupervisor.props(createKafkaProducer()),
                ClusterShardingSettings.create(system),
                messageExtractor
        );
    }

    private static KafkaProducer<String, String> createKafkaProducer() {
        Properties producerProps = new Properties();
        producerProps.put(ProducerConfig.BOOTSTRAP_SERVERS_CONFIG, BOOTSTRAP_SERVERS);
        producerProps.put(ProducerConfig.KEY_SERIALIZER_CLASS_CONFIG, StringSerializer.class.getName());
        producerProps.put(ProducerConfig.VALUE_SERIALIZER_CLASS_CONFIG, StringSerializer.class.getName());
        producerProps.put(ProducerConfig.TRANSACTIONAL_ID_CONFIG, TRANSACTIONAL_ID + "-sharding");
        producerProps.put(ProducerConfig.ENABLE_IDEMPOTENCE_CONFIG, "true");

        KafkaProducer<String, String> producer = new KafkaProducer<>(producerProps);
        producer.initTransactions();
        return producer;
    }

    /**
     * Determines if a reset signal should be triggered based on data characteristics
     * Your reset logic can be implemented here
     */
    private static boolean shouldTriggerReset(String value, String sensorType) {
        try {
            double numericValue = Double.parseDouble(value);
            
            // Example reset conditions:
            switch (sensorType) {
                case "temperature":
                    // Reset if temperature is unrealistic (outside -50 to 100°C)
                    return numericValue < -50 || numericValue > 100;
                case "humidity":
                    // Reset if humidity is outside 0-100%
                    return numericValue < 0 || numericValue > 100;
                case "wind":
                    // Reset if wind speed is unrealistic (>200 km/h)
                    return numericValue < 0 || numericValue > 200;
                case "airQuality":
                    // Reset if air quality index is unrealistic
                    return numericValue < 0 || numericValue > 500;
                default:
                    return false;
            }
        } catch (NumberFormatException e) {
            // Reset on invalid numeric data
            return true;
        }
    }
}
