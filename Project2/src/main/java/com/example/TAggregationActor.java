package com.example;

import akka.actor.AbstractActorWithStash;
import akka.actor.Props;

import org.apache.kafka.clients.producer.KafkaProducer;
import org.apache.kafka.clients.producer.ProducerRecord;

import java.util.ArrayList;
import java.util.List;

public class TAggregationActor extends AbstractActorWithStash {
    private final List<Double> window = new ArrayList<>();
    private final int windowSize;
    private final KafkaProducer<String, String> producer;
    private final String outputTopic;
    private final String aggregationType; // e.g., "average", "sum", etc.
    private final String outputKey; // e.g., "temperature", "humidity", etc.

    public AggregationActor(int windowSize, KafkaProducer<String, String> producer, String outputTopic, String aggregationType, String outputKey) {
        this.windowSize = windowSize;
        this.producer = producer;
        this.outputTopic = outputTopic;
        this.aggregationType = aggregationType;
        this.outputKey = outputKey;
    }

    public static Props props(int windowSize, KafkaProducer<String, String> producer, String outputTopic, String aggregationType, String outputKey) {
        return Props.create(TAggregationActor.class, () -> new TAggregationActor(windowSize, producer, outputTopic,aggregationType, outputKey));
    }

    @Override
    public Receive createReceive() {
        return receiveBuilder()
            .match(Double.class, this::processValue)
            .match(flushWindow.class, this::flushWindow)
            .build();
    }
    
    private void flushWindow() {
        if (window.isEmpty()) {
            return;
        }
        
        double result;
        switch (aggregationType) {
            case "average":
                result = window.stream().mapToDouble(Double::doubleValue).average().orElse(0);
                break;
            case "sum":
                result = window.stream().mapToDouble(Double::doubleValue).sum();
                break;
            case "max":
                result = window.stream().mapToDouble(Double::doubleValue).max().orElse(0);
                break;
            case "min":
                result = window.stream().mapToDouble(Double::doubleValue).min().orElse(0);
                break;
            default:
                throw new IllegalArgumentException("Unknown aggregation type: " + aggregationType);

            }   
        sendToKafka(result);
        window.clear();
        unstashAll();
    }

    private void sendToKafka(double value) {
        String message = String.format("{\"aggregationType\": \"%s\", \"value\": %f}", aggregationType, value);
        ProducerRecord<String, String> record = new ProducerRecord<>(outputTopic, outputKey, message);
        producer.send(record, (metadata, exception) -> {
            if (exception != null) {
                System.err.println("Error sending message to Kafka: " + exception.getMessage());
            } else {
                System.out.printf("Sent to Kafka -> Topic: %s, Offset: %d\n", metadata.topic(), metadata.offset());
            }
        });
    }

    private void processValue(Double value) {
        window.add(value);
        if (window.size() == windowSize) {
            flushWindow();
        }
        else {
            stash();
        }
        
    }

    public static class flushWindow {
        // This class can be empty, it's just a marker for the message type
    }
}

