package com.example;

import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonProperty;

import java.util.List;

public class WindowSnapshot {
    private final List<Double> window;

    @JsonCreator
    public WindowSnapshot(@JsonProperty("window") List<Double> window) {
        this.window = window;
    }

    public List<Double> getWindow() {
        return window;
    }
}