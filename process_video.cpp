#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <cstdlib>
#include <chrono>
#include <iomanip>

namespace fs = std::filesystem;

// Get video height using ffprobe
int getVideoHeight(const std::string& videoPath) {
    std::string cmd = "ffprobe -v error -select_streams v:0 -show_entries stream=height "
                      "-of default=noprint_wrappers=1:nokey=1 \"" + videoPath + "\"";
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) return -1;
    char buffer[128];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    _pclose(pipe);
    try {
        return std::stoi(result);
    } catch (...) {
        return -1;
    }
}

// Get subordinate qualities based on input resolution (YouTube style)
std::vector<std::pair<std::string, int>> getSubordinateQualities(int inputHeight) {
    std::vector<std::pair<std::string, int>> allQualities = {
        {"2160", 2160}, // 4K
        {"1440", 1440}, // 2K
        {"1080", 1080}, // Full HD
        {"720", 720},   // HD
        {"480", 480},   // SD
        {"360", 360},   // Low
        {"240", 240},   // Very Low
        {"144", 144}    // Lowest
    };
    
    std::vector<std::pair<std::string, int>> subordinateQualities;
    
    // Only include qualities that are lower than input resolution
    for (const auto& quality : allQualities) {
        if (quality.second < inputHeight) {
            subordinateQualities.push_back(quality);
        }
    }
    
    return subordinateQualities;
}

void processVideo(const std::string& videoPath) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    fs::path inputPath(videoPath);
    std::string stem = inputPath.stem().string();
    std::string folderName = stem;

    // Create output folder
    fs::create_directory(folderName);

    // Get input video height
    int inputHeight = getVideoHeight(videoPath);
    if (inputHeight == -1) {
        std::cerr << "Could not determine input video height.\n";
        return;
    }
    
    std::cout << "Input video resolution: " << inputHeight << "p" << std::endl;
    
    // Copy original video with height in filename
    std::string originalOut = folderName + "/" + stem + " " + std::to_string(inputHeight) + ".mp4";
    fs::copy_file(videoPath, originalOut, fs::copy_options::overwrite_existing);
    std::cout << "Original copied as: " << originalOut << std::endl;

    // Get subordinate qualities
    std::vector<std::pair<std::string, int>> subordinateQualities = getSubordinateQualities(inputHeight);
    
    if (subordinateQualities.empty()) {
        std::cout << "No subordinate qualities to process for " << inputHeight << "p video." << std::endl;
        return;
    }
    
    std::cout << "Processing subordinate qualities: ";
    for (const auto& q : subordinateQualities) {
        std::cout << q.first << "p ";
    }
    std::cout << std::endl;

    // Process each subordinate quality
    for (const auto& q : subordinateQualities) {
        std::string outFile = folderName + "/" + stem + " " + q.first + ".mp4";
        std::string cmd = "ffmpeg -y -i \"" + videoPath + "\" -vf \"scale=-2:" + std::to_string(q.second) + "\" -c:a copy \"" + outFile + "\"";
        std::cout << "Processing " << q.first << "p..." << std::endl;
        
        int result = system(cmd.c_str());
        if (result == 0) {
            std::cout << "✓ " << q.first << "p completed" << std::endl;
        } else {
            std::cout << "✗ " << q.first << "p failed" << std::endl;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    double total_time = duration.count() / 1000.0;

    std::cout << "\nProcessing complete. Files saved in folder: " << folderName << std::endl;
    std::cout << "Total processing time: " << std::fixed << std::setprecision(2) << total_time << " seconds" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: process_video <video_path>\n";
        std::cerr << "Example: process_video.exe video.mp4\n";
        return 1;
    }
    
    if (!fs::exists(argv[1])) {
        std::cerr << "Error: File does not exist: " << argv[1] << std::endl;
        return 1;
    }
    
    processVideo(argv[1]);
    return 0;
}