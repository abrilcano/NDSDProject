package com.polimi.utils;

import java.io.Serializable;

public class DataMessage implements Serializable {
    
    private final double value;
    private final String aggregationType; // e.g., "average", "sum", etc.
    private final String sensorType;

    public DataMessage(double value, String aggregationType, String sensorType) {
        this.value = value;
        this.aggregationType = aggregationType;
        this.sensorType = sensorType;
    }

    public double getValue() {
        return value;
    }

    public String getAggregationType() {
        return aggregationType;
    }

    public String getSensorType() {
        return sensorType;
    }
}


