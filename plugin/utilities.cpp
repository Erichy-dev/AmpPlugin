#include "utilities.h"
#include <string>
#include <fstream>
#include <ctime>
#include <cstring>

using namespace std;

// Debug logging function
void logDebug(const std::string& message) {
    string logPath;
#ifdef VDJ_WIN
    char* userProfile = getenv("USERPROFILE");
    if (userProfile) {
        logPath = string(userProfile) + "\\AppData\\Local\\VirtualDJ\\debug.log";
    } else {
        logPath = "C:\\temp\\amp_debug.log"; // fallback
    }
#else
    char* homeDir = getenv("HOME");
    if (homeDir) {
        logPath = string(homeDir) + "/Library/Application Support/VirtualDJ/debug.log";
    } else {
        logPath = "/tmp/amp_debug.log"; // fallback
    }
#endif
    
    ofstream logFile(logPath, ios::app);
    if (logFile.is_open()) {
        time_t now = time(0);
        char* timeStr = ctime(&now);
        // Remove newline from ctime
        if (timeStr && strlen(timeStr) > 0) {
            timeStr[strlen(timeStr) - 1] = '\0';
        }
        logFile << "[" << timeStr << "] " << message << endl;
        logFile.close();
    }
}
