package com.example;

import java.io.Serializable;

public class DataMessage implements Serializable {
    
    private final double value;
    private final String aggregationType; // e.g., "average", "sum", etc.
    private final String outputKey; // e.g., "temperature", "humidity", etc.
    private final String outputTopic;

    public DataMessage(double value, String aggregationType, String outputKey, String outputTopic) {
        this.value = value;
        this.aggregationType = aggregationType;
        this.outputKey = outputKey;
        this.outputTopic = outputTopic;
    }

    public double getValue() {
        return value;
    }

    public String getAggregationType() {
        return aggregationType;
    }

    public String getOutputKey() {
        return outputKey;
    }

    public String getOutputTopic() {
        return outputTopic;
    }
}


