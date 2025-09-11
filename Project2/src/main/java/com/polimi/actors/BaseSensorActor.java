package com.polimi.actors;

import com.polimi.utils.DataMessage;
import com.polimi.utils.WindowSnapshot;
import akka.persistence.AbstractPersistentActor;
import akka.persistence.SnapshotOffer;
import akka.actor.Props;
import org.apache.kafka.clients.producer.KafkaProducer;
import org.apache.kafka.clients.producer.ProducerRecord;
import java.util.ArrayList;
import java.util.List;
import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonProperty;

public abstract class BaseSensorActor extends AbstractPersistentActor {
    protected final List<Double> window = new ArrayList<>();
    protected final int windowSize;
    protected final int windowSlide;
    protected final KafkaProducer<String, String> producer;
    protected final String sensorType;
    protected String aggregationType;
    
    // Common event classes
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
    
    public static class FlushWindow {}
    
    public static class ResetWindow {} // NEW: Reset signal
    
    public BaseSensorActor(String sensorType, int windowSize, int windowSlide, 
                          KafkaProducer<String, String> producer) {
        this.sensorType = sensorType;
        this.windowSize = windowSize;
        this.windowSlide = windowSlide;
        this.producer = producer;
    }
    
    @Override
    public String persistenceId() {
        return sensorType + "-actor-" + getSelf().path().name();
    }
    
    @Override
    public Receive createReceiveRecover() {
        return receiveBuilder()
            .match(ValueAdded.class, event -> {
                System.out.println("Recovering event: " + event.getValue());
                window.add(event.getValue());
            })
            .match(FlushWindow.class, event -> window.clear())
            .match(ResetWindow.class, event -> window.clear())
            .match(SnapshotOffer.class, snapshot -> {
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
            .match(DataMessage.class, this::processData)
            .match(FlushWindow.class, msg -> flushWindow())
            .match(ResetWindow.class, msg -> resetWindow()) // NEW: Handle reset signal
            .build();
    }
    
    protected void resetWindow() {
        persist(new ResetWindow(), event -> {
            window.clear();
            saveSnapshot(new ArrayList<>(window));
            System.out.println(sensorType + " actor: Window reset to empty state");
        });
    }
    
    protected void flushWindow() {
        window.clear();
        persist(new FlushWindow(), event -> {
            window.clear();
            saveSnapshot(new ArrayList<>(window));
        });
    }
    
    protected void processWindow() {
        if (window.isEmpty()) {
            return;
        }
        
        double result = calculateAggregation();
        sendToNextStage(result);
        slideWindow();
        saveSnapshot(new ArrayList<>(window));
    }
    
    protected double calculateAggregation() {
        switch (aggregationType) {
            case "average":
                return window.stream().mapToDouble(Double::doubleValue).average().orElse(0);
            case "sum":
                return window.stream().mapToDouble(Double::doubleValue).sum();
            case "max":
                return window.stream().mapToDouble(Double::doubleValue).max().orElse(0);
            case "min":
                return window.stream().mapToDouble(Double::doubleValue).min().orElse(0);
            default:
                throw new IllegalArgumentException("Unknown aggregation type: " + aggregationType);
        }
    }
    
    protected void slideWindow() {
        for (int i = 0; i < windowSlide && !window.isEmpty(); i++) {
            window.remove(0);
        }
    }
    
    // Abstract method for next stage topic determination
    protected abstract String getNextStageTopic();
    protected abstract String transformKey(String currentKey, double result);
    
    protected void sendToNextStage(double result) {
        try {
            producer.beginTransaction();
            String message = String.format("{ \"%s_%s\": %f}", sensorType, aggregationType, result);
            String nextTopic = getNextStageTopic();
            String transformedKey = transformKey(sensorType, result);
            
            ProducerRecord<String, String> record = new ProducerRecord<>(nextTopic, transformedKey, message);
            producer.send(record);
            producer.commitTransaction();
            
            System.out.printf("Sent to Kafka -> Topic: %s, Key: %s, Value: %s\n", 
                            nextTopic, transformedKey, message);
        } catch (Exception e) {
            System.err.println("Error in transaction: " + e.getMessage());
            producer.abortTransaction();
        }
    }
    
    protected void processData(DataMessage msg) {
        this.aggregationType = msg.getAggregationType();
        
        persist(new ValueAdded(msg.getValue()), event -> {
            window.add(event.getValue());
            if (window.size() >= windowSize) {
                processWindow();
            } else {
                System.out.println("Window size not reached yet. Current size: " + window.size());
                System.out.println("Actor Received data: " + msg.getValue());
            }
        });
    }
}
