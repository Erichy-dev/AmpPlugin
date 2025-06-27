#include "../AMP.h"
#include "utilities.h"
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef VDJ_WIN
#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#elif defined(VDJ_MAC)
// For popen
#include <cstdio>
#endif


// HTTP GET implementation
std::string CAMP::httpGet(const std::string& url)
{
    logDebug("httpGet called with URL: " + url);
    std::string response;
    
#ifdef VDJ_WIN
    logDebug("httpGet: Using Windows WinINet");
    HINTERNET hInternet = InternetOpenA("VDJ Plugin", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet) {
        logDebug("httpGet: Internet connection opened");
        HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
        if (hUrl) {
            logDebug("httpGet: URL opened, reading data");
            char buffer[4096];
            DWORD bytesRead;
            while (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                response += buffer;
            }
            logDebug("httpGet: Data read, response length: " + std::to_string(response.length()));
            InternetCloseHandle(hUrl);
        } else {
            logDebug("httpGet: Failed to open URL");
        }
        InternetCloseHandle(hInternet);
    } else {
        logDebug("httpGet: Failed to open internet connection");
    }
#elif defined(VDJ_MAC)
    // HTTP request using macOS system command
    logDebug("httpGet: Using macOS curl");
    std::string command = "curl -s \"" + url + "\"";
    logDebug("httpGet: Executing command: " + command);
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        logDebug("httpGet: Command executed, reading output");
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            response += buffer;
        }
        pclose(pipe);
        logDebug("httpGet: Command completed, response length: " + std::to_string(response.length()));
    } else {
        logDebug("httpGet: Failed to execute curl command");
    }
#endif
    
    logDebug("httpGet completed, returning response");
    return response;
}

// Simple JSON parsing for our specific response format
std::vector<TrackInfo> CAMP::parseTracksFromJson(const std::string& jsonString)
{
    logDebug("parseTracksFromJson called with JSON length: " + std::to_string(jsonString.length()));
    std::vector<TrackInfo> tracks;
    

    // Parse new API format - look for "tracks" array
    logDebug("parseTracksFromJson: Looking for 'tracks' array in JSON");
    size_t tracksPos = jsonString.find("\"tracks\"");
    if (tracksPos == std::string::npos) {
        // If we can't find "tracks", create a test track to show parsing failed
        logDebug("parseTracksFromJson: 'tracks' not found in JSON, creating error track");
        TrackInfo testTrack;
        testTrack.uniqueId = "parse_error";
        testTrack.name = "Please subscribe to AMP";
        testTrack.directory = "Error";
        testTrack.url = "https://music.abelldjcompany.com/audio/test.mp3";
        testTrack.size = 0;
        tracks.push_back(testTrack);
        logDebug("parseTracksFromJson: Returning error track");
        return tracks;
    }
    
    size_t arrayStart = jsonString.find('[', tracksPos);
    if (arrayStart == std::string::npos) {
        logDebug("parseTracksFromJson: Array start '[' not found, creating error track");
        TrackInfo testTrack;
        testTrack.uniqueId = "parse_error2";
        testTrack.name = "Please subscribe to AMP";
        testTrack.directory = "Error";
        testTrack.url = "https://music.abelldjcompany.com/audio/test.mp3";
        testTrack.size = 0;
        tracks.push_back(testTrack);
        logDebug("parseTracksFromJson: Returning array error track");
        return tracks;
    }
    
    // Parse the real JSON response - look for each track object
    logDebug("parseTracksFromJson: Found tracks array, starting to parse tracks");
    size_t pos = arrayStart + 1;
    int trackCount = 0;
    
    while (pos < jsonString.length() && trackCount < 5000) { // Higher limit for all tracks
        size_t objStart = jsonString.find('{', pos);
        if (objStart == std::string::npos) break;
        
        // Find the matching closing brace
        int braceCount = 1;
        size_t objEnd = objStart + 1;
        while (objEnd < jsonString.length() && braceCount > 0) {
            if (jsonString[objEnd] == '{') braceCount++;
            else if (jsonString[objEnd] == '}') braceCount--;
            objEnd++;
        }
        objEnd--; // Point to the closing brace
        
        if (braceCount != 0) break; // Malformed JSON
        
        std::string trackObj = jsonString.substr(objStart, objEnd - objStart + 1);
        
        TrackInfo track;
        track.size = 0;
        
        // Extract fileName for name
        size_t fileNameStart = trackObj.find("\"fileName\":");
        if (fileNameStart != std::string::npos) {
            fileNameStart = trackObj.find('"', fileNameStart + 11) + 1;
            size_t fileNameEnd = trackObj.find('"', fileNameStart);
            if (fileNameEnd != std::string::npos) {
                track.name = trackObj.substr(fileNameStart, fileNameEnd - fileNameStart);
            }
        }
        
        // Extract cleanPath for uniqueId
        size_t pathStart = trackObj.find("\"cleanPath\":");
        if (pathStart != std::string::npos) {
            pathStart = trackObj.find('"', pathStart + 12) + 1;
            size_t pathEnd = trackObj.find('"', pathStart);
            if (pathEnd != std::string::npos) {
                track.uniqueId = trackObj.substr(pathStart, pathEnd - pathStart);
            }
        }
        
        // Extract fieldName as directory
        size_t fieldStart = trackObj.find("\"fieldName\":");
        if (fieldStart != std::string::npos) {
            fieldStart = trackObj.find('"', fieldStart + 12) + 1;
            size_t fieldEnd = trackObj.find('"', fieldStart);
            if (fieldEnd != std::string::npos) {
                track.directory = trackObj.substr(fieldStart, fieldEnd - fieldStart);
            }
        }
        
        // Extract fullUrl
        size_t urlStart = trackObj.find("\"fullUrl\":");
        if (urlStart != std::string::npos) {
            urlStart = trackObj.find('"', urlStart + 10) + 1;
            size_t urlEnd = trackObj.find('"', urlStart);
            if (urlEnd != std::string::npos) {
                track.url = trackObj.substr(urlStart, urlEnd - urlStart);
            }
        }
        
        // Only add track if we have essential fields
        if (!track.name.empty() && !track.uniqueId.empty() && !track.url.empty()) {
            tracks.push_back(track);
            trackCount++;
        }
        
        pos = objEnd + 1;
    }
    
    logDebug("parseTracksFromJson completed: parsed " + std::to_string(tracks.size()) + " tracks");
    return tracks;
}

// Authentication helper methods
std::string CAMP::httpGetWithAuth(const std::string& url)
{
    logDebug("httpGetWithAuth called with URL: " + url);
    
    if (apiKey.empty()) {
        apiKey = getStoredApiKey();
    }
    
    if (apiKey.empty()) {
        logDebug("httpGetWithAuth: No API key available");
        return "";
    }
    
    std::string response;
    
#ifdef VDJ_WIN
    logDebug("httpGetWithAuth: Using Windows WinINet with API key");
    HINTERNET hInternet = InternetOpenA("VDJ Plugin", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet) {
        HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
        if (hUrl) {
            // Add API key header
            std::string header = "x-api-key: " + apiKey;
            HttpAddRequestHeadersA(hUrl, header.c_str(), header.length(), HTTP_ADDREQ_FLAG_ADD);
            
            char buffer[4096];
            DWORD bytesRead;
            while (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                response += buffer;
            }
            InternetCloseHandle(hUrl);
        }
        InternetCloseHandle(hInternet);
    }
#elif defined(VDJ_MAC)
    logDebug("httpGetWithAuth: Using macOS curl with API key");
    std::string command = "curl --location -s --header \"x-api-key: " + apiKey + "\" \"" + url + "\"";
    logDebug("httpGetWithAuth: Executing command: " + command);
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            response += buffer;
        }
        pclose(pipe);
    }
#endif
    
    logDebug("httpGetWithAuth completed, response length: " + std::to_string(response.length()));
    return response;
}

void CAMP::httpPost(const std::string& url, const std::string& postData)
{
    logDebug("httpPost called with URL: " + url + " and data: " + postData);

    if (apiKey.empty()) {
        apiKey = getStoredApiKey();
    }
    
    if (apiKey.empty()) {
        logDebug("httpPost: No API key available");
        return;
    }

#ifdef VDJ_MAC
    // Using curl for POST request on macOS
    std::string command = "curl -s -X POST -H \"Content-Type: application/json\" -H \"x-api-key: " + apiKey + "\" -d '" + postData + "' \"" + url + "\"";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        logDebug("httpPost: failed to execute curl command.");
        return;
    }
    
    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
    }
    
    pclose(pipe);
    logDebug("httpPost response: " + result);
#elif defined(VDJ_WIN)
    // Using WinINet for POST request on Windows
    HINTERNET hInternet = InternetOpenA("VDJ Plugin", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        logDebug("httpPost: InternetOpenA failed.");
        return;
    }

    URL_COMPONENTS urlComp;
    char szHostName[256];
    char szUrlPath[2048];

    memset(&urlComp, 0, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.lpszHostName = szHostName;
    urlComp.dwHostNameLength = sizeof(szHostName);
    urlComp.lpszUrlPath = szUrlPath;
    urlComp.dwUrlPathLength = sizeof(szUrlPath);
    
    if (!InternetCrackUrlA(url.c_str(), url.length(), 0, &urlComp)) {
        logDebug("httpPost: InternetCrackUrlA failed.");
        InternetCloseHandle(hInternet);
        return;
    }

    HINTERNET hConnect = InternetConnectA(hInternet, urlComp.lpszHostName, urlComp.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        logDebug("httpPost: InternetConnectA failed.");
        InternetCloseHandle(hInternet);
        return;
    }

    const char* acceptTypes[] = {"application/json", NULL};
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", urlComp.lpszUrlPath, NULL, NULL, acceptTypes, (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? INTERNET_FLAG_SECURE : 0, 0);
    if (!hRequest) {
        logDebug("httpPost: HttpOpenRequestA failed.");
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return;
    }

    std::string headers = "Content-Type: application/json\r\nx-api-key: " + apiKey;
    if (!HttpSendRequestA(hRequest, headers.c_str(), headers.length(), (LPVOID)postData.c_str(), postData.length())) {
         DWORD dwError = GetLastError();
        logDebug("httpPost: HttpSendRequestA failed. Error: " + std::to_string(dwError));
    }
    
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
#endif
}

bool CAMP::validateApiKey(const std::string& key)
{
    logDebug("validateApiKey called");
    if (key.empty()) return false;
    
    // Test the API key by making a request to the fields endpoint
    std::string testUrl = "https://music.abelldjcompany.com/api/fields-db";
    
#ifdef VDJ_MAC
    std::string command = "curl --location -s --header \"x-api-key: " + key + "\" \"" + testUrl + "\"";
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[256];
        std::string response;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            response += buffer;
        }
        pclose(pipe);
        
        // If we get a valid JSON response with fields, the key works
        bool valid = response.find("\"fields\"") != std::string::npos;
        logDebug("validateApiKey result: " + std::string(valid ? "valid" : "invalid"));
        return valid;
    }
#endif
    
    return false; // Default to invalid if we can't test
}

std::string CAMP::getStoredApiKey()
{
    logDebug("getStoredApiKey called");
    // Simple file-based storage for now
    std::string keyPath;
    
#ifdef VDJ_WIN
    char* userProfile = getenv("USERPROFILE");
    if (userProfile) {
        keyPath = std::string(userProfile) + "\\AppData\\Local\\VirtualDJ\\.camp_session_cache";
    }
#else
    char* homeDir = getenv("HOME");
    if (homeDir) {
        keyPath = std::string(homeDir) + "/Library/Application Support/VirtualDJ/.camp_session_cache";
    }
#endif
    
    if (keyPath.empty()) {
        logDebug("getStoredApiKey: Could not determine key storage path");
        return "";
    }
    
    std::ifstream keyFile(keyPath);
    if (keyFile.is_open()) {
        std::string key;
        getline(keyFile, key);
        keyFile.close();
        logDebug("getStoredApiKey: Retrieved API key from storage");
        return key;
    }
    
    logDebug("getStoredApiKey: No stored API key found");
    return "";
}

void CAMP::storeApiKey(const std::string& key)
{
    logDebug("storeApiKey called");
    std::string keyPath;
    
#ifdef VDJ_WIN
    char* userProfile = getenv("USERPROFILE");
    if (userProfile) {
        keyPath = std::string(userProfile) + "\\AppData\\Local\\VirtualDJ\\.camp_session_cache";
    }
#else
    char* homeDir = getenv("HOME");
    if (homeDir) {
        keyPath = std::string(homeDir) + "/Library/Application Support/VirtualDJ/.camp_session_cache";
    }
#endif
    
    if (!keyPath.empty()) {
        std::ofstream keyFile(keyPath);
        if (keyFile.is_open()) {
            keyFile << key;
            keyFile.close();
            logDebug("storeApiKey: API key stored successfully");
        }
    }
}

void CAMP::clearApiKey()
{
    logDebug("clearApiKey called");
    std::string keyPath;
    
#ifdef VDJ_WIN
    char* userProfile = getenv("USERPROFILE");
    if (userProfile) {
        keyPath = std::string(userProfile) + "\\AppData\\Local\\VirtualDJ\\.camp_session_cache";
    }
#else
    char* homeDir = getenv("HOME");
    if (homeDir) {
        keyPath = std::string(homeDir) + "/Library/Application Support/VirtualDJ/.camp_session_cache";
    }
#endif
    
    if (!keyPath.empty()) {
        remove(keyPath.c_str());
        logDebug("clearApiKey: API key file removed");
    }
}

bool CAMP::downloadFile(const std::string& url, const std::string& filePath, const std::string& apiKey)
{
    logDebug("downloadFile called. URL: " + url + ", Path: " + filePath);

#ifdef VDJ_WIN
    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile.is_open()) {
        logDebug("downloadFile: Failed to open file for writing: " + filePath);
        return false;
    }

    HINTERNET hInternet = InternetOpenA("VDJ Plugin", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        logDebug("downloadFile: InternetOpenA failed.");
        outFile.close();
        return false;
    }
    
    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hUrl) {
        logDebug("downloadFile: InternetOpenUrlA failed for url: " + url);
        InternetCloseHandle(hInternet);
        outFile.close();
        return false;
    }

    if (!apiKey.empty()) {
        std::string header = "x-api-key: " + apiKey;
        HttpAddRequestHeadersA(hUrl, header.c_str(), header.length(), HTTP_ADDREQ_FLAG_ADD);
    }
    
    char buffer[4096];
    DWORD bytesRead;
    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        outFile.write(buffer, bytesRead);
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    outFile.close();

    logDebug("downloadFile: File downloaded successfully to: " + filePath);
    return true;
#elif defined(VDJ_MAC)
    std::string command = "curl --location -s --header \"x-api-key: " + apiKey + "\" -o \"" + filePath + "\" \"" + url + "\"";
    
    int result = system(command.c_str());
    
    if (result == 0) {
        logDebug("downloadFile: File downloaded successfully to: " + filePath);
        return true;
    } else {
        logDebug("downloadFile: curl command failed with exit code: " + std::to_string(result));
        return false;
    }
#else
    return false; 
#endif
}

std::string CAMP::urlEncode(const std::string& value)
{
    std::string encoded;
    encoded.reserve(value.size() * 3); // Reserve space for worst case (all chars encoded)
    
    for (unsigned char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            // These characters don't need encoding according to RFC 3986
            encoded += c;
        } else if (c == ' ') {
            // Space becomes %20
            encoded += "%20";
        } else {
            // Other characters need to be percent-encoded
            encoded += '%';
            encoded += "0123456789ABCDEF"[c >> 4];
            encoded += "0123456789ABCDEF"[c & 15];
        }
    }
    
    return encoded;
}

// Search result limit configuration functions
int CAMP::getSearchResultLimit()
{
    if (searchResultLimit <= 0) {
        searchResultLimit = getStoredSearchResultLimit();
    }
    return searchResultLimit;
}

void CAMP::setSearchResultLimit(int limit)
{
    if (limit > 0 && limit <= 1000) { // Reasonable bounds
        searchResultLimit = limit;
        storeSearchResultLimit(limit);
    }
}

int CAMP::getStoredSearchResultLimit()
{
    logDebug("getStoredSearchResultLimit called");
    std::string settingsPath;
    
#ifdef VDJ_WIN
    char* userProfile = getenv("USERPROFILE");
    if (userProfile) {
        settingsPath = std::string(userProfile) + "\\AppData\\Local\\VirtualDJ\\.camp_search_limit";
    }
#else
    char* homeDir = getenv("HOME");
    if (homeDir) {
        settingsPath = std::string(homeDir) + "/Library/Application Support/VirtualDJ/.camp_search_limit";
    }
#endif
    
    if (settingsPath.empty()) {
        logDebug("getStoredSearchResultLimit: Could not determine settings storage path");
        return 50; // Default
    }
    
    std::ifstream settingsFile(settingsPath);
    if (settingsFile.is_open()) {
        std::string limitStr;
        getline(settingsFile, limitStr);
        settingsFile.close();
        
        int limit = std::stoi(limitStr);
        if (limit > 0 && limit <= 1000) {
            logDebug("getStoredSearchResultLimit: Retrieved limit from storage: " + std::to_string(limit));
            return limit;
        }
    }
    
    logDebug("getStoredSearchResultLimit: Using default limit: 50");
    return 50; // Default
}

void CAMP::storeSearchResultLimit(int limit)
{
    logDebug("storeSearchResultLimit called with limit: " + std::to_string(limit));
    std::string settingsPath;
    
#ifdef VDJ_WIN
    char* userProfile = getenv("USERPROFILE");
    if (userProfile) {
        settingsPath = std::string(userProfile) + "\\AppData\\Local\\VirtualDJ\\.camp_search_limit";
    }
#else
    char* homeDir = getenv("HOME");
    if (homeDir) {
        settingsPath = std::string(homeDir) + "/Library/Application Support/VirtualDJ/.camp_search_limit";
    }
#endif
    
    if (!settingsPath.empty()) {
        std::ofstream settingsFile(settingsPath);
        if (settingsFile.is_open()) {
            settingsFile << std::to_string(limit);
            settingsFile.close();
            logDebug("storeSearchResultLimit: Limit stored successfully");
        }
    }
}
