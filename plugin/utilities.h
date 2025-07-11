#ifndef VDJ_UTILITIES_H
#define VDJ_UTILITIES_H

#include <string>

// Debug logging function declaration
void logDebug(const std::string& message);

// Track parsing functions
std::pair<std::string, std::string> parseTrackTitleAndArtist(const std::string& trackName);
std::string truncateString(const std::string& str, size_t maxLength);

#endif // VDJ_UTILITIES_H 
