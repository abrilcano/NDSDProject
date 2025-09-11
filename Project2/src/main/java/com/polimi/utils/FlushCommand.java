package com.polimi.utils;

import java.io.Serializable;

/**
 * Message to trigger window flush/reset for a specific sensor type
 * Routes through the sharding system like DataMessage
 */
public class FlushCommand implements Serializable {
    private final String sensorType;
    private final String reason;
    private final long timestamp;
    
    public FlushCommand(String sensorType, String reason) {
        this.sensorType = sensorType;
        this.reason = reason;
        this.timestamp = System.currentTimeMillis();
    }
    
    public FlushCommand(String sensorType) {
        this(sensorType, "Manual flush");
    }
    
    public String getSensorType() {
        return sensorType;
    }
    
    public String getReason() {
        return reason;
    }
    
    public long getTimestamp() {
        return timestamp;
    }
    
    @Override
    public String toString() {
        return String.format("FlushCommand{sensorType='%s', reason='%s', timestamp=%d}", 
                           sensorType, reason, timestamp);
    }
}
