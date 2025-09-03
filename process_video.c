#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <direct.h>   // _mkdir
    #define MKDIR(path) _mkdir(path)
    #define COPY_CMD "copy /Y \"%s\" \"%s\""   // Windows copy
    #define PATH_SEP '\\'
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #define MKDIR(path) mkdir(path, 0777)
    #define COPY_CMD "cp \"%s\" \"%s\""       // Linux/macOS copy
    #define PATH_SEP '/'
#endif

// Get video height using ffprobe
int getVideoHeight(const char *videoPath) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "ffprobe -v error -select_streams v:0 -show_entries stream=height "
             "-of default=noprint_wrappers=1:nokey=1 \"%s\"",
             videoPath);

    FILE *pipe = popen(cmd, "r");
    if (!pipe) return -1;

    char buffer[128];
    if (!fgets(buffer, sizeof(buffer), pipe)) {
        pclose(pipe);
        return -1;
    }
    pclose(pipe);

    return atoi(buffer);
}

// Quality structure
typedef struct {
    const char *label;
    int height;
} Quality;

// Get subordinate qualities based on input resolution
int getSubordinateQualities(int inputHeight, Quality *outQualities, int maxCount) {
    Quality allQualities[] = {
        {"2160", 2160}, // 4K
        {"1440", 1440}, // 2K
        {"1080", 1080}, // Full HD
        {"720", 720},   // HD
        {"480", 480},   // SD
        {"360", 360},   // Low
        {"240", 240},   // Very Low
        {"144", 144}    // Lowest
    };

    int total = sizeof(allQualities) / sizeof(allQualities[0]);
    int count = 0;

    for (int i = 0; i < total; i++) {
        if (allQualities[i].height < inputHeight) {
            if (count < maxCount) {
                outQualities[count++] = allQualities[i];
            }
        }
    }
    return count;
}

// Extract filename without extension
void getStem(const char *videoPath, char *stem, size_t size) {
    const char *lastSep = strrchr(videoPath, PATH_SEP);
    const char *fileName = lastSep ? lastSep + 1 : videoPath;

    strncpy(stem, fileName, size - 1);
    stem[size - 1] = '\0';

    char *dot = strrchr(stem, '.');
    if (dot) *dot = '\0';
}

// Process video
void processVideo(const char *videoPath) {
    clock_t start_time = clock();
    
    char stem[256];
    getStem(videoPath, stem, sizeof(stem));

    // Create output folder
    MKDIR(stem);

    // Get input height
    int inputHeight = getVideoHeight(videoPath);
    if (inputHeight == -1) {
        fprintf(stderr, "Could not determine input video height.\n");
        return;
    }

    printf("Input video resolution: %dp\n", inputHeight);

    // Copy original video
    char originalOut[512];
    snprintf(originalOut, sizeof(originalOut), "%s%c%s %d.mp4", stem, PATH_SEP, stem, inputHeight);

    char copyCmd[1024];
    snprintf(copyCmd, sizeof(copyCmd), COPY_CMD, videoPath, originalOut);
    system(copyCmd);
    printf("Original copied as: %s\n", originalOut);

    // Get subordinate qualities
    Quality qualities[10];
    int count = getSubordinateQualities(inputHeight, qualities, 10);

    if (count == 0) {
        printf("No subordinate qualities to process for %dp video.\n", inputHeight);
        return;
    }

    printf("Processing subordinate qualities: ");
    for (int i = 0; i < count; i++) {
        printf("%sp ", qualities[i].label);
    }
    printf("\n");

    // Process each subordinate quality
    for (int i = 0; i < count; i++) {
        char outFile[512];
        snprintf(outFile, sizeof(outFile), "%s%c%s %s.mp4", stem, PATH_SEP, stem, qualities[i].label);

        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
                 "ffmpeg -y -i \"%s\" -vf \"scale=-2:%d\" -c:a copy \"%s\"",
                 videoPath, qualities[i].height, outFile);

        printf("Processing %sp...\n", qualities[i].label);
        int result = system(cmd);
        if (result == 0) {
            printf("✓ %sp completed\n", qualities[i].label);
        } else {
            printf("✗ %sp failed\n", qualities[i].label);
        }
    }

    clock_t end_time = clock();
    double total_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    printf("\nProcessing complete. Files saved in folder: %s\n", stem);
    printf("Total processing time: %.2f seconds\n", total_time);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <video_path>\n", argv[0]);
        fprintf(stderr, "Example: %s video.mp4\n", argv[0]);
        return 1;
    }

    // Check if file exists
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "Error: File does not exist: %s\n", argv[1]);
        return 1;
    }
    fclose(f);

    processVideo(argv[1]);
    return 0;
}
