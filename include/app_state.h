#pragma once

// Forward declaration
class App;

/**
 * Abstract Base Class for Application States
 * Implements the State Pattern
 */
class AppState {
public:
    virtual ~AppState() = default;
    
    // Called when entering the state
    virtual void enter(App& app) = 0;
    
    // Called every loop iteration
    virtual void update(App& app) = 0;
    
    // Called when a refresh is requested asynchronously
    virtual void onRefresh(App& app) {}

    // Called when leaving the state
    virtual void exit(App& app) = 0;
};
