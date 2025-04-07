package com.example;

import akka.persistence.AbstractPersistentActor;
import akka.persistence.SnapshotOffer;
import akka.actor.Props;

import org.apache.kafka.clients.producer.KafkaProducer;
import org.apache.kafka.clients.producer.ProducerRecord;

import java.util.ArrayList;
import java.util.List;

import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonProperty;


public class TempActor extends AbstractPersistentActor {
    private final List<Double> window = new ArrayList<>();
    private final int windowSize;
    private final int windowSlide;
    private final KafkaProducer<String, String> producer;
    private String aggregationType; // e.g., "average", "sum", etc.
    // private String outputKey; // e.g., "temperature", "humidity", etc.
    // private String outputTopic;

    // Event to make the Actor fault tolerant
    public static class ValueAdded {
        private final double value;

        @JsonCreator
        public ValueAdded(@JsonProperty("value") double value) {
            this.value = value;
        }

        public double getValue() {
            return value;
        }
    }

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
    public String persistenceId() {
        return "temp-actor-" + getSelf().path().name();
    }

    @Override
    public Receive createReceiveRecover() {
        return receiveBuilder()
            .match(ValueAdded.class, event -> {
                System.out.println("Recovering event: " + event.getValue());
                window.add(event.getValue());
            })
            .match(FlushWindow.class, event -> {
                window.clear();
            })
            .match(SnapshotOffer.class, snapshot -> {
                // Restore state from snapshot
                WindowSnapshot snapshotState = (WindowSnapshot) snapshot.snapshot();
                System.out.println("Restoring state from snapshot: " + snapshotState.getWindow());
                window.clear();
                window.addAll(snapshotState.getWindow());
            })
            .build();
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
            // .match(SaveSnapshotSuccess.class, msg -> {
            //     System.out.println("Snapshot saved successfully: " + msg.metadata().sequenceNr());
            // })
            // .match(SaveSnapshotFailure.class, msg -> {
            //     System.err.println("Failed to save snapshot: " + msg.cause().getMessage());
            // })
            .build();
    }

    private void flushWindow(){
        window.clear();
        persist(new FlushWindow(), event -> {
            window.clear();
            saveSnapshot(new ArrayList<>(window)); // Save the current state as a snapshot
        });
        // unstashAll();
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

        saveSnapshot(new ArrayList<>(window)); // Save the current state as a snapshot

        //window.clear();
        //unstashAll();
    }


    private void sendToKafka(double value) {
        try{
            producer.beginTransaction();
            String message = String.format("{ %s temperature: %f}", aggregationType, value);
            // ProducerRecord<String, String> record = new ProducerRecord<>(outputTopic, outputKey, message);
            ProducerRecord<String, String> record = new ProducerRecord<>("output", "output", message);

            producer.send(record);
            producer.commitTransaction();
            System.out.printf("Sent to Kafka -> Topic: %s, Key: %s, Value: %s\n", "output", "output", message);
        }catch (Exception e){
            System.err.println("Error in transaction: " + e.getMessage());
            producer.abortTransaction();

        }

    }

    private void processData(DataMessage msg) {
        this.aggregationType = msg.getAggregationType();
        // this.outputKey = msg.getOutputKey();
        // this.outputTopic = msg.getOutputTopic();

        persist(new ValueAdded(msg.getValue()), event -> {
            // This is where you would handle the event after persisting it
            window.add(event.getValue());
            if (window.size() == windowSize) {
                processWindow();
            }
            else {
                System.out.println("Window size not reached yet. Current size: " + window.size());
                System.out.println("Actor Recieved data: " + msg.getValue());
                //stash();
            }
        });



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
