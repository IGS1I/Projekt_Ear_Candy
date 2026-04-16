#ifndef MP3_MANAGER_H
#define MP3_MANAGER_H

#include <iostream>
#include <vector>
#include <string>

class MP3Manager {
private:
    std::vector<std::string> trackList;
    int currentTrackIndex;

public:
    MP3Manager() : currentTrackIndex(0) {}

    // Adds a filename to the list
    void addTrack(const std::string& fileName) {
        // Simple check for audio extensions
        if (fileName.find(".mp3") != std::string::npos || fileName.find(".wav") != std::string::npos) {
            trackList.push_back(fileName);
        }
    }

    void nextTrack() {
        if (trackList.empty()) return;
        currentTrackIndex = (currentTrackIndex + 1) % trackList.size();
    }

    void prevTrack() {
        if (trackList.empty()) return;
        currentTrackIndex = (currentTrackIndex - 1 + trackList.size()) % trackList.size();
    }

    std::string getCurrentTrack() const {
        if (trackList.empty()) return "No Tracks Found";
        return trackList[currentTrackIndex];
    }
};

#endif
