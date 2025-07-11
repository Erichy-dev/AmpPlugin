#include "utilities.h"
#include <string>
#include <fstream>
#include <ctime>
#include <cstring>
#include <regex>

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

// Truncate string to specified length
std::string truncateString(const std::string& str, size_t maxLength) {
    if (str.length() <= maxLength) {
        return str;
    }
    return str.substr(0, maxLength - 3) + "...";
}

// Parse track title and artist from filename
std::pair<std::string, std::string> parseTrackTitleAndArtist(const std::string& trackName) {
    string filename = trackName;
    string title = "";
    string artist = "";
    
    // Keep the full track name as title (including .mp3)
    title = trackName;
    
    // Work with filename without extension for artist parsing
    size_t dotPos = filename.find_last_of('.');
    if (dotPos != string::npos) {
        filename = filename.substr(0, dotPos);
    }
    
    // Remove leading ._ if present
    if (filename.substr(0, 2) == "._") {
        filename = filename.substr(2);
    }
    
    // Remove leading - if present
    if (filename.substr(0, 2) == "- ") {
        filename = filename.substr(2);
    }
    
    // Pattern 1: (Year/Number/Genre) Artist - Title [Remix/Mix]
    regex pattern1(R"(\([^)]+\)\s*(.+?)\s*-\s*(.+?)(?:\s*[\[\(][^)\]]*[\]\)])?$)");
    smatch match1;
    if (regex_match(filename, match1, pattern1)) {
        artist = match1[1].str();
    }
    // Pattern 2: Artist - Title (without prefix)
    else {
        regex pattern2(R"((.+?)\s*-\s*(.+?)(?:\s*[\[\(][^)\]]*[\]\)])?$)");
        smatch match2;
        if (regex_match(filename, match2, pattern2)) {
            artist = match2[1].str();
        }
        // Pattern 3: No dash separator, try to extract from brackets
        else {
            regex pattern3(R"(\[([^\]]+)\]\s*(.+)$)");
            smatch match3;
            if (regex_match(filename, match3, pattern3)) {
                artist = match3[1].str();
            }
            // Pattern 4: Default - use "Unknown" as artist
            else {
                artist = "Unknown";
            }
        }
    }
    
    // Clean up artist
    artist = regex_replace(artist, regex(R"(^\s+|\s+$)"), ""); // trim
    
    // Remove feat./featuring from artist
    artist = regex_replace(artist, regex(R"(\s*[,&]\s*.*)", regex_constants::icase), "");
    artist = regex_replace(artist, regex(R"(\s+feat\.?\s+.*)", regex_constants::icase), "");
    
    // Final cleanup for artist
    artist = regex_replace(artist, regex(R"(^\s+|\s+$)"), "");
    
    // Truncate artist to keep concise
    artist = truncateString(artist, 25);
    
    // Handle empty cases
    if (artist.empty()) artist = "Unknown";
    if (title.empty()) title = trackName; // fallback to original track name
    
    return make_pair(title, artist);
}
