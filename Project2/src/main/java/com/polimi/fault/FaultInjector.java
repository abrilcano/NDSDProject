package com.polimi.fault;

import java.util.Random;

public class FaultInjector {
    private static final Random random = new Random();
    
    public static void injectProcessingFault(double probability) {
        if (random.nextDouble() < probability) {
            System.err.println("FAULT INJECTION: Processing failure!");
            throw new RuntimeException("Simulated processing failure");
        }
    }
    
    public static void injectNetworkFault(double probability) {
        if (random.nextDouble() < probability) {
            System.err.println("FAULT INJECTION: Network failure!");
            throw new RuntimeException("Simulated network failure");
        }
    }
    
    public static void injectMemoryFault(double probability) {
        if (random.nextDouble() < probability) {
            System.err.println("FAULT INJECTION: Memory failure!");
            throw new OutOfMemoryError("Simulated memory failure");
        }
    }
    
    public static void injectKafkaFault(double probability) {
        if (random.nextDouble() < probability) {
            System.err.println("FAULT INJECTION: Kafka failure!");
            throw new RuntimeException("Simulated Kafka failure");
        }
    }
}
