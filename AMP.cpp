#include "AMP.h"
#define FTS_FUZZY_MATCH_IMPLEMENTATION
#include "fts_fuzzy_match.h"
#include <string>
#include <algorithm>
#include <sstream>
#include <vector>
#include <utility>
#include <fstream>
#include <ctime>
#include <set>
#include <thread>

#ifdef VDJ_WIN
#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#elif defined(VDJ_MAC)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

using namespace std;

// Debug logging function
void logDebug(const string& message) {
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

HRESULT VDJ_API CAMP::OnGetPluginInfo(TVdjPluginInfo8* infos)
{
    logDebug("OnGetPluginInfo called");
    infos->PluginName = "AMP";
    infos->Author = "Abell DJ Company";
    infos->Description = "AMP Music Pool";
    infos->Version = "1.0";
    infos->Flags = 0;
    infos->Bitmap = NULL;
    logDebug("OnGetPluginInfo completed - plugin info set");
    return S_OK;
}

// Login implementation
HRESULT VDJ_API CAMP::IsLogged()
{
    logDebug("IsLogged called");
    apiKey = getStoredApiKey();
    bool logged = !apiKey.empty();
    logDebug("IsLogged result: " + string(logged ? "true" : "false"));
    return logged ? S_OK : S_FALSE;
}

HRESULT VDJ_API CAMP::OnLogin()
{
    logDebug("OnLogin called");
    
    // For now, use the default API key for testing
    // In production, you'd prompt the user for their API key
    string testKey = "amp-default-key-123";
    
    if (validateApiKey(testKey)) {
        storeApiKey(testKey);
        apiKey = testKey;
        logDebug("Login successful with test API key");
        return S_OK;
    }
    
    logDebug("Login failed - invalid API key");
    return S_FALSE;
}

HRESULT VDJ_API CAMP::OnLogout()
{
    logDebug("OnLogout called");
    clearApiKey();
    apiKey.clear();
    tracksCached = false;
    cachedTracks.clear();
    logDebug("Logout completed");
    return S_OK;
}

HRESULT VDJ_API CAMP::OnSearch(const char* search, IVdjTracksList* tracksList)
{
    logDebug("OnSearch called with search term: " + string(search ? search : "(null)"));

    if (!search || strlen(search) == 0) {
        logDebug("Empty search term, returning empty results");
        return S_OK;
    }

    if (apiKey.empty()) {
        apiKey = getStoredApiKey();
    }
    if (apiKey.empty()) {
        logDebug("User not logged in - no API key available for search.");
        return S_OK;
    }

    string encodedSearch = urlEncode(search);
    string searchUrl = "https://music.abelldjcompany.com/api/search?q=" + encodedSearch;

    logDebug("Performing search with URL: " + searchUrl);

    string jsonResponse = httpGetWithAuth(searchUrl);
    if (jsonResponse.empty()) {
        logDebug("Empty JSON response from search API");
        return S_OK;
    }

    // Parse tracks from JSON response
    logDebug("Parsing search results from JSON response");
    vector<TrackInfo> searchResults = parseTracksFromJson(jsonResponse);
    if (searchResults.empty()) {
        logDebug("No tracks found from search API.");
        return S_OK;
    }

    logDebug("Adding " + to_string(searchResults.size()) + " search results");
    for (const auto& track : searchResults) {
        string artist = track.directory;
        if (artist.empty() || artist == "/") {
            artist = "Unknown Artist";
        }

        const char* streamUrl = nullptr;
        string localPath;
        if (isTrackCached(track.uniqueId.c_str())) {
            localPath = getEncodedLocalPathForTrack(track.uniqueId.c_str());
            streamUrl = localPath.c_str();
        }

        tracksList->add(
            track.uniqueId.c_str(),   // uniqueId
            track.name.c_str(),       // title
            artist.c_str(),           // artist
            "",                       // remix
            nullptr,                  // genre
            "Music Pool",             // label
            "Search result",          // comment
            "",                       // cover URL
            streamUrl,                // streamUrl
            0,                        // length
            0,                        // bpm
            0,                        // key
            0,                        // year
            false,                    // isVideo
            false                     // isKaraoke
        );
    }

    logDebug("OnSearch completed with " + to_string(searchResults.size()) + " results");
    return S_OK;
}

HRESULT VDJ_API CAMP::OnSearchCancel()
{
    logDebug("OnSearchCancel called");
    // Internet::closeDownloads();
    logDebug("OnSearchCancel completed");
    return S_OK;
}

HRESULT VDJ_API CAMP::GetStreamUrl(const char* uniqueId, IVdjString& url, IVdjString& errorMessage)
{
    string id = uniqueId ? uniqueId : "(null)";
    logDebug("GetStreamUrl called with uniqueId: '" + id + "'");
    
    // First, check if the track is cached locally
    if (isTrackCached(uniqueId)) {
        string localPath = getEncodedLocalPathForTrack(uniqueId);
        logDebug("Track is cached. Returning local path: " + localPath);
        url = localPath.c_str();
        return S_OK;
    }
    
    // If not cached, look for the track in our full track list to get the remote URL
    logDebug("Track not cached. Searching in memory...");
    ensureTracksAreCached();
    for (const auto& track : cachedTracks) {
        if (track.uniqueId == id) {
            logDebug("Found track in memory: " + track.url);
            url = track.url.c_str();
            return S_OK;
        }
    }
    
    // If not found in cache or memory, construct URL directly as a fallback with proper URL encoding
    if (!id.empty() && id != "fallback") {
        string encodedPath = urlEncode(id);
        string streamUrl = "https://music.abelldjcompany.com/audio/" + encodedPath;
        logDebug("Constructed fallback stream URL: " + streamUrl);
        url = streamUrl.c_str();
        return S_OK;
    }
    
    logDebug("Track not found, returning error");
    errorMessage = "Track not found";
    return S_FALSE;
}

HRESULT VDJ_API CAMP::GetFolderList(IVdjSubfoldersList* subfoldersList)
{
    logDebug("GetFolderList called");

    // Fetch fields/folders from new API endpoint
    if (apiKey.empty()) {
        apiKey = getStoredApiKey();
    }

    if (apiKey.empty()) {
        logDebug("User not logged in - no API key available. Please log in first.");
        return S_OK;
    }

    logDebug("Fetching fields from API");
    string jsonResponse = httpGetWithAuth("https://music.abelldjcompany.com/api/fields-db");
    if (jsonResponse.empty()) {
        logDebug("Empty JSON response from fields API");
        return S_OK;
    }

    // Parse fields from JSON response
    logDebug("Parsing fields from JSON response");
    size_t fieldsPos = jsonResponse.find("\"fields\"");
    if (fieldsPos == std::string::npos) {
        logDebug("'fields' not found in JSON response");
        return S_OK;
    }

    size_t arrayStart = jsonResponse.find('[', fieldsPos);
    if (arrayStart == std::string::npos) {
        logDebug("Fields array start '[' not found");
        return S_OK;
    }
    
    // Parse each field object
    size_t pos = arrayStart + 1;
    int fieldCount = 0;
    
    while (pos < jsonResponse.length() && fieldCount < 1000) {
        size_t objStart = jsonResponse.find('{', pos);
        if (objStart == std::string::npos) break;

        // Find the matching closing brace
        int braceCount = 1;
        size_t objEnd = objStart + 1;
        while (objEnd < jsonResponse.length() && braceCount > 0) {
            if (jsonResponse[objEnd] == '{') braceCount++;
            else if (jsonResponse[objEnd] == '}') braceCount--;
            objEnd++;
        }
        objEnd--; // Point to the closing brace

        if (braceCount != 0) break; // Malformed JSON

        std::string fieldObj = jsonResponse.substr(objStart, objEnd - objStart + 1);

        // Extract field name
        size_t nameStart = fieldObj.find("\"name\":");
        if (nameStart != std::string::npos) {
            nameStart = fieldObj.find('"', nameStart + 7) + 1;
            size_t nameEnd = fieldObj.find('"', nameStart);
            if (nameEnd != std::string::npos) {
                string fieldName = fieldObj.substr(nameStart, nameEnd - nameStart);

                // Extract path count for display
                size_t pathCountStart = fieldObj.find("\"pathCount\":");
                string displayName = fieldName;
                if (pathCountStart != std::string::npos) {
                    pathCountStart += 12; // Skip "pathCount":
                    size_t pathCountEnd = fieldObj.find_first_of(",}", pathCountStart);
                    if (pathCountEnd != std::string::npos) {
                        string pathCountStr = fieldObj.substr(pathCountStart, pathCountEnd - pathCountStart);
                        displayName = fieldName + " (" + pathCountStr + ")";
                    }
                }

                subfoldersList->add(fieldName.c_str(), displayName.c_str());
                fieldCount++;
            }
        }

        pos = objEnd + 1;
    }

    logDebug("GetFolderList completed - added " + to_string(fieldCount) + " fields");
    return S_OK;
}

HRESULT VDJ_API CAMP::GetFolder(const char* folderUniqueId, IVdjTracksList* tracksList)
{
    string folderId = folderUniqueId ? folderUniqueId : "(null)";
    logDebug("GetFolder called with folderUniqueId: '" + folderId + "'");
    
    if (apiKey.empty()) {
        apiKey = getStoredApiKey();
    }
    
    if (apiKey.empty()) {
        logDebug("User not logged in - no API key available. Please log in first.");
        return S_OK;
    }

    // Fetch tracks for specific field from new API endpoint
    string apiUrl = "https://music.abelldjcompany.com/api/fields/" + folderId + "/tracks";
    logDebug("Fetching tracks from: " + apiUrl);
    string jsonResponse = httpGetWithAuth(apiUrl);
    if (jsonResponse.empty()) {
        logDebug("Empty JSON response from field tracks API");
        return S_OK;
    }

    // Parse tracks from JSON response
    logDebug("Parsing tracks from JSON response");
    size_t tracksPos = jsonResponse.find("\"tracks\"");
    if (tracksPos == std::string::npos) {
        logDebug("'tracks' not found in JSON response");
        return S_OK;
    }
    
    size_t arrayStart = jsonResponse.find('[', tracksPos);
    if (arrayStart == std::string::npos) {
        logDebug("Tracks array start '[' not found");
        return S_OK;
    }
    
    // Parse each track object
    size_t pos = arrayStart + 1;
    int trackCount = 0;
    
    while (pos < jsonResponse.length() && trackCount < 1000) {
        size_t objStart = jsonResponse.find('{', pos);
        if (objStart == std::string::npos) break;

        // Find the matching closing brace
        int braceCount = 1;
        size_t objEnd = objStart + 1;
        while (objEnd < jsonResponse.length() && braceCount > 0) {
            if (jsonResponse[objEnd] == '{') braceCount++;
            else if (jsonResponse[objEnd] == '}') braceCount--;
            objEnd++;
        }
        objEnd--; // Point to the closing brace

        if (braceCount != 0) break; // Malformed JSON

        std::string trackObj = jsonResponse.substr(objStart, objEnd - objStart + 1);

        string fileName, fullUrl, cleanPath;

        // Extract fileName
        size_t fileNameStart = trackObj.find("\"fileName\":");
        if (fileNameStart != std::string::npos) {
            fileNameStart = trackObj.find('"', fileNameStart + 11) + 1;
            size_t fileNameEnd = trackObj.find('"', fileNameStart);
            if (fileNameEnd != std::string::npos) {
                fileName = trackObj.substr(fileNameStart, fileNameEnd - fileNameStart);
            }
        }

        // Extract fullUrl
        size_t urlStart = trackObj.find("\"fullUrl\":");
        if (urlStart != std::string::npos) {
            urlStart = trackObj.find('"', urlStart + 10) + 1;
            size_t urlEnd = trackObj.find('"', urlStart);
            if (urlEnd != std::string::npos) {
                fullUrl = trackObj.substr(urlStart, urlEnd - urlStart);
            }
        }

        // Extract cleanPath for uniqueId
        size_t pathStart = trackObj.find("\"cleanPath\":");
        if (pathStart != std::string::npos) {
            pathStart = trackObj.find('"', pathStart + 12) + 1;
            size_t pathEnd = trackObj.find('"', pathStart);
            if (pathEnd != std::string::npos) {
                cleanPath = trackObj.substr(pathStart, pathEnd - pathStart);
            }
        }

        // Only add track if we have essential fields
        if (!fileName.empty() && !fullUrl.empty() && !cleanPath.empty()) {
            const char* streamUrl = nullptr;
            string localPath;
            if (isTrackCached(cleanPath.c_str())) {
                localPath = getEncodedLocalPathForTrack(cleanPath.c_str());
                streamUrl = localPath.c_str();
            }
            
            tracksList->add(
                cleanPath.c_str(),        // uniqueId (cleanPath)
                fileName.c_str(),         // title (fileName)
                folderId.c_str(),         // artist (field name)
                "",                       // remix
                nullptr,                  // genre
                "Music Pool",             // label
                "",                       // comment
                "",                       // cover URL
                streamUrl,                // streamUrl
                0,                        // length (determined when loaded)
                0,                        // bpm
                0,                        // key
                0,                        // year
                false,                    // isVideo
                false                     // isKaraoke
            );
            trackCount++;
        }

        pos = objEnd + 1;
    }
    
    logDebug("GetFolder completed, added " + to_string(trackCount) + " tracks to folder '" + folderId + "'");
    return S_OK;
}

HRESULT VDJ_API CAMP::GetContextMenu(const char* uniqueId, IVdjContextMenu* contextMenu)
{
    string id = uniqueId ? uniqueId : "(null)";
    logDebug("GetContextMenu called with uniqueId: '" + id + "'");

    if (isTrackCached(uniqueId)) {
        contextMenu->add("Delete from Cache");
    } else {
        contextMenu->add("Download to Cache");
    }

    logDebug("GetContextMenu completed");
    return S_OK;
}

HRESULT VDJ_API CAMP::OnContextMenu(const char* uniqueId, size_t menuIndex)
{
    string id = uniqueId ? uniqueId : "(null)";
    logDebug("OnContextMenu called with uniqueId: '" + id + "', menuIndex: " + to_string(menuIndex));
    
    if (menuIndex == 0) {
        if (isTrackCached(uniqueId)) {
            logDebug("'Delete from Cache' selected for: " + id);
            deleteTrackFromCache(uniqueId);
        } else {
            logDebug("'Download to Cache' selected for: " + id);
            downloadTrackToCache(uniqueId);
        }
    }

    logDebug("OnContextMenu completed");
    return S_OK;
}

HRESULT VDJ_API CAMP::GetFolderContextMenu(const char *folderUniqueId, IVdjContextMenu *contextMenu)
{
    string folderId = folderUniqueId ? folderUniqueId : "(null)";
    logDebug("GetFolderContextMenu called with folderUniqueId: '" + folderId + "'");
    contextMenu->add("GetFolderContextMenu");
    logDebug("GetFolderContextMenu completed");
    return S_OK;
}

HRESULT VDJ_API CAMP::OnFolderContextMenu(const char *folderUniqueId, size_t menuIndex)
{
    string folderId = folderUniqueId ? folderUniqueId : "(null)";
    logDebug("OnFolderContextMenu called with folderUniqueId: '" + folderId + "', menuIndex: " + to_string(menuIndex));
    logDebug("OnFolderContextMenu completed");
    return S_OK;
}

void CAMP::downloadTrackToCache(const char* uniqueId)
{
    logDebug("downloadTrackToCache called for: " + string(uniqueId ? uniqueId : "(null)"));
    if (!uniqueId) return;

    if (isTrackCached(uniqueId)) {
        logDebug("Track is already cached. Skipping download.");
        return;
    }

    ensureTracksAreCached();

    string downloadUrl;

    // Try to find the track in the master cache first
    const TrackInfo* trackToDownload = nullptr;
    for (const auto& track : cachedTracks) {
        if (track.uniqueId == uniqueId) {
            trackToDownload = &track;
            break;
        }
    }

    if (trackToDownload) {
        logDebug("Found track in memory to download: " + trackToDownload->name);
        downloadUrl = trackToDownload->url;
    } else {
        // If not found, construct the URL as a fallback (similar to GetStreamUrl)
        logDebug("Track not found in memory cache. Constructing fallback URL.");
        string id = uniqueId;
        if (!id.empty() && id != "fallback") {
            string encodedPath = urlEncode(id);
            downloadUrl = "https://music.abelldjcompany.com/audio/" + encodedPath;
        }
    }

    if (!downloadUrl.empty()) {
        string filePath = getCachePathForTrack(uniqueId);
        if (filePath.empty()) {
            logDebug("Could not get a valid cache path. Aborting download.");
            return;
        }

        logDebug("Spawning a background thread to download from URL: " + downloadUrl);

        // Copy variables to pass to the thread
        string uniqueIdStr = uniqueId;
        string apiKeyCopy = this->apiKey;

        std::thread([this, downloadUrl, filePath, apiKeyCopy, uniqueIdStr]() {
            if (downloadFile(downloadUrl, filePath, apiKeyCopy)) {
                logDebug("Background download successful for uniqueId: " + uniqueIdStr);
                if (this->cb) {
                    this->cb->SendCommand("browser_refresh");
                }
            } else {
                logDebug("Background download failed for uniqueId: " + uniqueIdStr);
                remove(filePath.c_str());
            }
        }).detach();

    } else {
        logDebug("Could not determine a download URL for uniqueId: " + string(uniqueId));
    }
}

void CAMP::deleteTrackFromCache(const char* uniqueId)
{
    logDebug("deleteTrackFromCache called for: " + string(uniqueId ? uniqueId : "(null)"));
    if (!uniqueId) return;

    if (isTrackCached(uniqueId)) {
        string filePath = getCachePathForTrack(uniqueId);
        if (!filePath.empty()) {
            if (remove(filePath.c_str()) == 0) {
                logDebug("Successfully deleted cached track: " + filePath);
            } else {
                logDebug("Error deleting cached track: " + filePath);
            }
        }
    } else {
        logDebug("Track not in cache, nothing to delete.");
    }
}

bool CAMP::isTrackCached(const char* uniqueId)
{
    string path = getCachePathForTrack(uniqueId);
    if (path.empty()) {
        return false;
    }

    ifstream f(path.c_str());
    if (f.good()) {
        f.close();
        return true;
    }
    
    return false;
}

std::string CAMP::getCacheDir()
{
    string base_path;
#ifdef VDJ_WIN
    char* userProfile = getenv("USERPROFILE");
    if (userProfile) {
        base_path = string(userProfile) + "\\AppData\\Local\\VirtualDJ";
    }
#else
    char* homeDir = getenv("HOME");
    if (homeDir) {
        base_path = string(homeDir) + "/Library/Application Support/VirtualDJ";
    }
#endif
    if(base_path.empty()) return "";

#ifdef VDJ_WIN
    string cache_path = base_path + "\\Cache";
    CreateDirectoryA(cache_path.c_str(), NULL);
    string amp_cache_path = cache_path + "\\AMP";
    CreateDirectoryA(amp_cache_path.c_str(), NULL);
    return amp_cache_path;
#else
    string cache_path = base_path + "/Cache";
    mkdir(cache_path.c_str(), 0777);
    string amp_cache_path = cache_path + "/AMP";
    mkdir(amp_cache_path.c_str(), 0777);
    return amp_cache_path;
#endif
}

std::string CAMP::getCachePathForTrack(const char* uniqueId)
{
    if (!uniqueId || strlen(uniqueId) == 0) return "";

    string cacheDir = getCacheDir();
    if (cacheDir.empty()) {
        return "";
    }

    string safeFileName = uniqueId;
    // Replace all slashes with underscore for a flat cache structure.
    for (char &c : safeFileName) {
        if (c == '/' || c == '\\') {
            c = '_';
        }
    }

#ifdef VDJ_WIN
    return cacheDir + "\\" + safeFileName;
#else
    return cacheDir + "/" + safeFileName;
#endif
}

std::string CAMP::getEncodedLocalPathForTrack(const char* uniqueId)
{
    string path = getCachePathForTrack(uniqueId);
    if (path.empty()) {
        return "";
    }

    // URL encode the path, especially for spaces.
    string encodedPath;
    encodedPath.reserve(path.size() * 3);
    for (char c : path) {
        if (c == ' ') {
            encodedPath += "%20";
        } else if (c == '/') {
            encodedPath += "/";
        } else if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encodedPath += c;
        }
        else {
            char buf[4];
            sprintf(buf, "%%%02X", (unsigned char)c);
            encodedPath += buf;
        }
    }
    
    return "file://" + encodedPath;
}

// Caching helper method
void CAMP::ensureTracksAreCached()
{
    if (tracksCached) {
        return;
    }

    logDebug("No cached tracks available, fetching from backend");

    // Check if user is logged in first
    if (apiKey.empty()) {
        apiKey = getStoredApiKey();
    }
    
    if (apiKey.empty()) {
        logDebug("User not logged in - no API key available. Please log in first.");
        return;
    }

    // Fetch tracks from backend using new API
    logDebug("Fetching tracks from backend with authentication");
    string jsonResponse = httpGetWithAuth("https://music.abelldjcompany.com/api/tracks");
    if (!jsonResponse.empty()) {
        logDebug("Received JSON response, parsing tracks");
        cachedTracks = parseTracksFromJson(jsonResponse);
        tracksCached = true;
        logDebug("Tracks cached successfully, count: " + to_string(cachedTracks.size()));
    } else {
        logDebug("Empty JSON response from backend - authentication may have failed");
    }
}

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
            logDebug("httpGet: Data read, response length: " + to_string(response.length()));
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
        logDebug("httpGet: Command completed, response length: " + to_string(response.length()));
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
    logDebug("parseTracksFromJson called with JSON length: " + to_string(jsonString.length()));
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
    
    logDebug("parseTracksFromJson completed: parsed " + to_string(tracks.size()) + " tracks");
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
            string header = "x-api-key: " + apiKey;
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
    string command = "curl --location -s --header \"x-api-key: " + apiKey + "\" \"" + url + "\"";
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
    
    logDebug("httpGetWithAuth completed, response length: " + to_string(response.length()));
    return response;
}

bool CAMP::validateApiKey(const std::string& key)
{
    logDebug("validateApiKey called");
    if (key.empty()) return false;
    
    // Test the API key by making a request to the fields endpoint
    string testUrl = "https://music.abelldjcompany.com/api/fields-db";
    
#ifdef VDJ_MAC
    string command = "curl --location -s --header \"x-api-key: " + key + "\" \"" + testUrl + "\"";
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[256];
        string response;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            response += buffer;
        }
        pclose(pipe);
        
        // If we get a valid JSON response with fields, the key works
        bool valid = response.find("\"fields\"") != string::npos;
        logDebug("validateApiKey result: " + string(valid ? "valid" : "invalid"));
        return valid;
    }
#endif
    
    return false; // Default to invalid if we can't test
}

std::string CAMP::getStoredApiKey()
{
    logDebug("getStoredApiKey called");
    // Simple file-based storage for now
    string keyPath;
    
#ifdef VDJ_WIN
    char* userProfile = getenv("USERPROFILE");
    if (userProfile) {
        keyPath = string(userProfile) + "\\AppData\\Local\\VirtualDJ\\.camp_session_cache";
    }
#else
    char* homeDir = getenv("HOME");
    if (homeDir) {
        keyPath = string(homeDir) + "/Library/Application Support/VirtualDJ/.camp_session_cache";
    }
#endif
    
    if (keyPath.empty()) {
        logDebug("getStoredApiKey: Could not determine key storage path");
        return "";
    }
    
    ifstream keyFile(keyPath);
    if (keyFile.is_open()) {
        string key;
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
    string keyPath;
    
#ifdef VDJ_WIN
    char* userProfile = getenv("USERPROFILE");
    if (userProfile) {
        keyPath = string(userProfile) + "\\AppData\\Local\\VirtualDJ\\.camp_session_cache";
    }
#else
    char* homeDir = getenv("HOME");
    if (homeDir) {
        keyPath = string(homeDir) + "/Library/Application Support/VirtualDJ/.camp_session_cache";
    }
#endif
    
    if (!keyPath.empty()) {
        ofstream keyFile(keyPath);
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
    string keyPath;
    
#ifdef VDJ_WIN
    char* userProfile = getenv("USERPROFILE");
    if (userProfile) {
        keyPath = string(userProfile) + "\\AppData\\Local\\VirtualDJ\\.camp_session_cache";
    }
#else
    char* homeDir = getenv("HOME");
    if (homeDir) {
        keyPath = string(homeDir) + "/Library/Application Support/VirtualDJ/.camp_session_cache";
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
    ofstream outFile(filePath, ios::binary);
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
        string header = "x-api-key: " + apiKey;
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
    string command = "curl --location -s --header \"x-api-key: " + apiKey + "\" -o \"" + filePath + "\" \"" + url + "\"";
    
    int result = system(command.c_str());
    
    if (result == 0) {
        logDebug("downloadFile: File downloaded successfully to: " + filePath);
        return true;
    } else {
        logDebug("downloadFile: curl command failed with exit code: " + to_string(result));
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
