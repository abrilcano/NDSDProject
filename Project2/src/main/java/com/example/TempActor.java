package com.example;

import akka.actor.AbstractActorWithStash;
import akka.actor.Props;

import org.apache.kafka.clients.producer.KafkaProducer;
import org.apache.kafka.clients.producer.ProducerRecord;

import java.util.ArrayList;
import java.util.List;

public class TempActor extends AbstractActorWithStash {
    private final List<Double> window = new ArrayList<>();
    private final int windowSize;
    private final int windowSlide; 
    private final KafkaProducer<String, String> producer;
    private String outputTopic;
    private String aggregationType; // e.g., "average", "sum", etc.
    private String outputKey; // e.g., "temperature", "humidity", etc.

    public static class FlushWindow {
        // This class can be empty, it's just a marker for the message type
    }

    public TempActor(int windowSize,int windowSlide, KafkaProducer<String, String> producer) {
        this.windowSize = windowSize;
        this.windowSlide = windowSlide;
        this.producer = producer;
    }

    public static Props props(int windowSize, int windowSlide, KafkaProducer<String, String> producer) {
        return Props.create(TempActor.class, () -> new TempActor(windowSize, windowSlide, producer));
    }

    @Override
    public Receive createReceive() {
        return receiveBuilder()
            .match(DataMessage.class, msg -> {
                processData(msg);
            })
            .match(FlushWindow.class, msg -> {
                flushWindow();
            })
            .build();
    }

    private void flushWindow(){
        window.clear();
        unstashAll();
    }

    private void processWindow(){
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

        // Slide the window
        for (int i = 0; i < windowSlide && !window.isEmpty() ; i++) {
            window.remove(0);
        }

        //window.clear();
        //unstashAll();
    }


    private void sendToKafka(double value) {
        try{
            producer.beginTransaction();
            String message = String.format("{ %s for temperature: %f}", aggregationType, value);
            // ProducerRecord<String, String> record = new ProducerRecord<>(outputTopic, outputKey, message);
            ProducerRecord<String, String> record = new ProducerRecord<>("output", "output", message);

            producer.send(record);
            producer.commitTransaction();
            System.out.printf("Sent to Kafka -> Topic: %s, Key: %s, Value: %s\n", outputTopic, outputKey, message);
        }catch (Exception e){
            System.err.println("Error in transaction: " + e.getMessage());
            producer.abortTransaction();

        }

    }

    private void processData(DataMessage msg) {
        this.aggregationType = msg.getAggregationType();
        this.outputKey = msg.getOutputKey();
        this.outputTopic = msg.getOutputTopic();

        window.add(msg.getValue());
        if (window.size() == windowSize) {
            processWindow();
        }
        else {
            System.out.println("Actor Recieved data: " + msg.getValue());
            //stash();
        }
        
    }


}

/*
 * 
 * public class FaultInjector {
    private static final Random random = new Random();
    
    public static void maybeFail(double failureProbability) {
        if (random.nextDouble() < failureProbability) {
            System.err.println("Injecting failure!");
            throw new RuntimeException("Simulated failure");
        }
    }
}

FaultInjector.maybeFail(0.01);
 */
