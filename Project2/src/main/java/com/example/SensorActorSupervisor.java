package com.example;

import akka.actor.*;
import scala.concurrent.duration.Duration;
import java.util.concurrent.TimeUnit;
import akka.japi.pf.DeciderBuilder;
import org.apache.kafka.clients.producer.KafkaProducer;

public class SensorActorSupervisor extends AbstractActor {
    private final KafkaProducer<String, String> producer;
    
    public SensorActorSupervisor(KafkaProducer<String, String> producer) {
        this.producer = producer;
    }
    
    public static Props props(KafkaProducer<String, String> producer) {
        return Props.create(SensorActorSupervisor.class, () -> new SensorActorSupervisor(producer));
    }
    
    @Override
    public SupervisorStrategy supervisorStrategy() {
        return new OneForOneStrategy(
            10, // Max retries
            Duration.create(1, TimeUnit.MINUTES), // Within this time window
            DeciderBuilder
                .match(RuntimeException.class, e -> {
                    System.err.println("Actor failed with RuntimeException: " + e.getMessage());
                    return SupervisorStrategy.restart();
                })
                .match(Exception.class, e -> {
                    System.err.println("Actor failed with Exception: " + e.getMessage());
                    return SupervisorStrategy.restart();
                })
                .matchAny(o -> SupervisorStrategy.escalate())
                .build()
        );
    }
    
    @Override
    public Receive createReceive() {
        return receiveBuilder()
            .match(CreateActorMessage.class, msg -> {
                ActorRef actor = null;
                switch (msg.getSensorType()) {
                    case "temperature":
                        actor = getContext().actorOf(
                            TempActor.props(msg.getWindowSize(), msg.getWindowSlide(), producer),
                            "temp-actor-" + msg.getActorId());
                        break;
                    // case "humidity":
                    //     actor = getContext().actorOf(
                    //         HumidityActor.props(msg.getWindowSize(), msg.getWindowSlide(), producer),
                    //         "humidity-actor-" + msg.getActorId());
                    //     break;
                    // case "wind":
                    //     actor = getContext().actorOf(
                    //         WindActor.props(msg.getWindowSize(), msg.getWindowSlide(), producer),
                    //         "wind-actor-" + msg.getActorId());
                    //     break;
                    // case "airQuality":
                    //     actor = getContext().actorOf(
                    //         AirQualityActor.props(msg.getWindowSize(), msg.getWindowSlide(), producer),
                    //         "air-actor-" + msg.getActorId());
                    //     break;
                    default:
                        throw new IllegalArgumentException("Unknown sensor type: " + msg.getSensorType());
                }
                getSender().tell(actor, getSelf());
            })
            .build();
    }
    
    // Message to create an actor
    public static class CreateActorMessage {
        private final String sensorType;
        private final String actorId;
        private final int windowSize;
        private final int windowSlide;
        
        public CreateActorMessage(String sensorType, String actorId, int windowSize, int windowSlide) {
            this.sensorType = sensorType;
            this.actorId = actorId;
            this.windowSize = windowSize;
            this.windowSlide = windowSlide;
        }
        
        public String getSensorType() { return sensorType; }
        public String getActorId() { return actorId; }
        public int getWindowSize() { return windowSize; }
        public int getWindowSlide() { return windowSlide; }
    }
}