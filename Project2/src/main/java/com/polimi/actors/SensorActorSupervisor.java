package com.polimi.actors;

import akka.actor.*;
import scala.concurrent.duration.Duration;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

import akka.japi.pf.DeciderBuilder;

import org.apache.kafka.clients.producer.KafkaProducer;

import com.polimi.utils.DataMessage;
import com.polimi.utils.FlushCommand;

public class SensorActorSupervisor extends AbstractActor {

    private final KafkaProducer<String, String> producer;
    private final Map<String, ActorRef> actorCache = new HashMap<>();
    private final Map<String, SensorConfig> sensorConfigMap = new HashMap<>();

    public SensorActorSupervisor(KafkaProducer<String, String> producer) {
        this.producer = producer;

        // Initialize configurations for each sensor type
        sensorConfigMap.put("temperature", new SensorConfig(10, 5));
        sensorConfigMap.put("humidity", new SensorConfig(15, 5));
        sensorConfigMap.put("wind", new SensorConfig(20, 10));
        sensorConfigMap.put("airQuality", new SensorConfig(30, 10));
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
                // Handle direct actor creation requests (from non-sharded systems)
                .match(CreateActorMessage.class, msg -> {
                    ActorRef actor = createActorForType(msg.getSensorType(), msg.getActorId());
                    getSender().tell(actor, getSelf());
                })
                // Handle sharded data messages (from cluster sharding)
                .match(DataMessage.class, msg -> {
                    // Get or create an appropriate actor for this sensor type
                    ActorRef actor = actorCache.computeIfAbsent(
                            msg.getSensorType(),
                            sensorType -> {
                                ActorRef newActor = createActorForType(sensorType, sensorType);
                                return newActor;
                            }
                    );

                    // Forward the message to the actor
                    actor.forward(msg, getContext());
                })
                // Handle flush commands (resets)
                .match(FlushCommand.class, msg -> {
                    // Get the existing actor for this sensor type
                    ActorRef actor = actorCache.get(msg.getSensorType());
                    if (actor != null) {
                        // Convert FlushCommand to FlushWindow message and forward
                        actor.tell(createFlushWindow(msg.getReason()), getSelf());
                        System.out.printf("Flush command forwarded to %s actor: %s\n", 
                                        msg.getSensorType(), msg.getReason());
                    } else {
                        System.out.printf("No actor found for sensor type: %s\n", msg.getSensorType());
                    }
                })
                .build();
    }

    private ActorRef createActorForType(String sensorType, String actorId) {
        SensorConfig config = sensorConfigMap.get(sensorType);
        if (config == null) {
            throw new IllegalArgumentException("Unknown sensor type: " + sensorType);
        }

        switch (sensorType) {
            case "temperature":
                return getContext().actorOf(
                        TempActor.props(config.getWindowSize(), config.getWindowSlide(), producer),
                        "temp-actor-" + actorId);
            case "humidity":
                return getContext().actorOf(
                        HumActor.props(config.getWindowSize(), config.getWindowSlide(), producer),
                        "hum-actor-" + actorId);
            case "wind":
                return getContext().actorOf(
                        WindActor.props(config.getWindowSize(), config.getWindowSlide(), producer),
                        "wind-actor-" + actorId);
            case "airQuality":
                return getContext().actorOf(
                        AirActor.props(config.getWindowSize(), config.getWindowSlide(), producer),
                        "air-actor-" + actorId);
            default:
                        throw new IllegalArgumentException("Unknown sensor type: " + sensorType);
        }
    }

    /**
     * Creates a FlushWindow message with the given reason
     * This needs to match the FlushWindow class of the target sensor actor
     */
    private Object createFlushWindow(String reason) {
        // Since all sensor actors have the same FlushWindow structure,
        // we can use TempActor's FlushWindow as a representative
        return new TempActor.FlushWindow(reason);
    }    // Message to create an actor
    public static class CreateActorMessage {

        private final String sensorType;
        private final String actorId;

        public CreateActorMessage(String sensorType, String actorId) {
            this.sensorType = sensorType;
            this.actorId = actorId;
        }

        public String getSensorType() {
            return sensorType;
        }

        public String getActorId() {
            return actorId;
        }
    }

    // Configuration class for sensor actors
    private static class SensorConfig {

        private final int windowSize;
        private final int windowSlide;

        public SensorConfig(int windowSize, int windowSlide) {
            this.windowSize = windowSize;
            this.windowSlide = windowSlide;
        }

        public int getWindowSize() {
            return windowSize;
        }

        public int getWindowSlide() {
            return windowSlide;
        }
    }
}
