#include <iostream>
#include <chrono>
#include <thread>
#include <gpiod.hpp>

class RadarProcessor{
private:
    int PulseCount;
    float ConversionFactor;

public:
    RadarProcessor(){
        PulseCount = 0;
        ConversionFactor = 31.36; // Target conversion factor for HB100 module
    }

    // Increment pulse counter by 1
    void RegisterPulse(){
        PulseCount++;
    }

    unsigned long getHzandReset(){
        unsigned long currentHz = PulseCount;
        PulseCount = 0;
        return currentHz;
    }

    float calculateSpeed(unsigned long frequencyHz){
        return (float)frequencyHz / ConversionFactor;
    }
};

int main(){
    RadarProcessor myRadar;

    const int RadarPin = 26;
    
    std::cout << "=========================================\n";
    std::cout << "  Initializing Radar Processor...   \n";
    std::cout << "=========================================\n";

    // 1. Open the Pi 5's main GPIO chip container
    gpiod::chip chip("/dev/gpiochip4"); 

    // 2. Claim your specific radar input pin using ACTUAL Version 2 syntax
    // Notice the extra '::' in line::direction and line::edge!
    auto request = chip.prepare_request()
        .set_consumer("RadarSpeedProcessor")
        .add_line_settings(
            RadarPin, 
            gpiod::line_settings()
                .set_direction(gpiod::line::direction::INPUT)
                .set_edge_detection(gpiod::line::edge::RISING)
        )
        .do_request();

    std::cout << "Radar active! System monitoring pin BCM " << RadarPin << "...\n";

    // Create a high-precision stopwatch
    auto start_time = std::chrono::steady_clock::now();

    // Infinite Timing Loop
    while(true){
        // 1. Wait for pulses (Timeout after 10 milliseconds so it doesn't freeze)
        if (request.wait_edge_events(std::chrono::milliseconds(10))) {
            gpiod::edge_event_buffer buffer;
            request.read_edge_events(buffer); 
            
            // Loop through the buffer and register every single pulse that hit
            for ([[maybe_unused]] const auto& event : buffer) {
                myRadar.RegisterPulse();
            }
        }

        // 2. Check the stopwatch: Has exactly 1 second (1000 milliseconds) passed?
        auto current_time = std::chrono::steady_clock::now();
        auto time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();

        if (time_elapsed >= 1000) {
            // 1 second has passed! Get the frequency and restart the stopwatch.
            unsigned long frequency = myRadar.getHzandReset();
            start_time = std::chrono::steady_clock::now();

            // 3. Process the data
            if (frequency > 0) {
                float targetSpeed = myRadar.calculateSpeed(frequency);
                std::cout << "Detected Signal: " << frequency << " Hz | ";
                std::cout << "Target Speed: " << targetSpeed << " mph\n";
            } else {
                std::cout << "Scanning... No target detected.\n";
            }
        }
    }
    
    return 0; 
}