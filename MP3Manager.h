#ifndef MP3_MANAGER_H
#define MP3_MANAGER_H

#include <cstdint>
#include <cstddef>

// Using a struct as requested—it's simpler for data-heavy tasks
struct MP3Metadata {
    // These are your two markers (The "Start" and "End" pointers)
    // They point to the location in memory where your ID3 data begins and ends
    const uint8_t* startPtr; 
    const uint8_t* endPtr;   

    // Function signature: This tells the program what this struct can do.
    // The actual logic (the for loop) lives in the .cpp file.
    void parseMP3(const uint8_t* fileBuffer, size_t bufferSize);
};

#endif