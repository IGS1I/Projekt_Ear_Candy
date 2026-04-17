#include "MP3Manager.h"
#include <cstdio>
#include <iostream>

using namespace std; 

void MP3Metadata::parseMP3(const uint8_t* fileBuffer, size_t bufferSize) {
    startPtr = nullptr;
    endPtr = nullptr;

    for (size_t i = 0; i < bufferSize - 3; i++) {
        if (fileBuffer[i] == 'I' && fileBuffer[i+1] == 'D' && fileBuffer[i+2] == '3') {
            startPtr = &fileBuffer[i];
            endPtr = startPtr + 128; 
            
        // Now you can use cout or printf easily (Output can be removed if neccessary)
            cout << "ID3 Tag found using namespace std!" << endl;
            return; 
        }
    }
}