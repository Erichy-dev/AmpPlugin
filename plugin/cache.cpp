#include "../AMP.h"
#include "utilities.h"
#include <string>
#include <vector>
#include <fstream>
#include <thread>
#include <cstring>
#include <cstdio>

#ifdef VDJ_WIN
#include <windows.h>
#elif defined(VDJ_MAC)
#include <sys/stat.h>
#endif

void CAMP::downloadTrackToCache(const char* uniqueId)
{
    logDebug("downloadTrackToCache called for: " + std::string(uniqueId ? uniqueId : "(null)"));
    if (!uniqueId) return;

    if (isTrackCached(uniqueId)) {
        logDebug("Track is already cached. Skipping download.");
        return;
    }

    ensureTracksAreCached();

    std::string downloadUrl;

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
        std::string id = uniqueId;
        if (!id.empty() && id != "fallback") {
            std::string encodedPath = urlEncode(id);
            downloadUrl = "https://music.abelldjcompany.com/audio/" + encodedPath;
        }
    }

    if (!downloadUrl.empty()) {
        std::string filePath = getCachePathForTrack(uniqueId);
        if (filePath.empty()) {
            logDebug("Could not get a valid cache path. Aborting download.");
            return;
        }

        logDebug("Spawning a background thread to download from URL: " + downloadUrl);

        // Copy variables to pass to the thread
        std::string uniqueIdStr = uniqueId;
        std::string apiKeyCopy = this->apiKey;

        std::thread([this, downloadUrl, filePath, apiKeyCopy, uniqueIdStr]() {
            if (downloadFile(downloadUrl, filePath, apiKeyCopy)) {
                logDebug("Background download successful for uniqueId: " + uniqueIdStr);
                cb->SendCommand("browsed_file_color \"#00FF00\"");
            } else {
                logDebug("Background download failed for uniqueId: " + uniqueIdStr);
                remove(filePath.c_str());
            }
        }).detach();

    } else {
        logDebug("Could not determine a download URL for uniqueId: " + std::string(uniqueId));
    }
}

void CAMP::deleteTrackFromCache(const char* uniqueId)
{
    logDebug("deleteTrackFromCache called for: " + std::string(uniqueId ? uniqueId : "(null)"));
    if (!uniqueId) return;

    if (isTrackCached(uniqueId)) {
        std::string filePath = getCachePathForTrack(uniqueId);
        if (!filePath.empty()) {
            if (remove(filePath.c_str()) == 0) {
                logDebug("Successfully deleted cached track: " + filePath);
                cb->SendCommand("browsed_file_color \"#D8D8D8\"");
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
    std::string path = getCachePathForTrack(uniqueId);
    if (path.empty()) {
        return false;
    }

    std::ifstream f(path.c_str());
    if (f.good()) {
        f.close();
        return true;
    }
    
    return false;
}

std::string CAMP::getCacheDir()
{
    std::string base_path;
#ifdef VDJ_WIN
    char* userProfile = getenv("USERPROFILE");
    if (userProfile) {
        base_path = std::string(userProfile) + "\\AppData\\Local\\VirtualDJ";
    }
#else
    char* homeDir = getenv("HOME");
    if (homeDir) {
        base_path = std::string(homeDir) + "/Library/Application Support/VirtualDJ";
    }
#endif
    if(base_path.empty()) return "";

#ifdef VDJ_WIN
    std::string cache_path = base_path + "\\Cache";
    CreateDirectoryA(cache_path.c_str(), NULL);
    std::string amp_cache_path = cache_path + "\\AMP";
    CreateDirectoryA(amp_cache_path.c_str(), NULL);
    return amp_cache_path;
#else
    std::string cache_path = base_path + "/Cache";
    mkdir(cache_path.c_str(), 0777);
    std::string amp_cache_path = cache_path + "/AMP";
    mkdir(amp_cache_path.c_str(), 0777);
    return amp_cache_path;
#endif
}

std::string CAMP::getCachePathForTrack(const char* uniqueId)
{
    if (!uniqueId || strlen(uniqueId) == 0) return "";

    std::string cacheDir = getCacheDir();
    if (cacheDir.empty()) {
        return "";
    }

    std::string safeFileName = uniqueId;
    // Replace problematic characters for a safe file name
    for (char &c : safeFileName) {
        if (c == '/' || c == '\\' || c == '$' || c == '?' || c == '*' || c == ':' || 
            c == '|' || c == '<' || c == '>' || c == '"' || c == '&' || c == '%') {
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
    std::string path = getCachePathForTrack(uniqueId);
    if (path.empty()) {
        return "";
    }

    // URL encode the path, especially for spaces.
    std::string encodedPath;
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
    std::string jsonResponse = httpGetWithAuth("https://music.abelldjcompany.com/api/tracks");
    if (!jsonResponse.empty()) {
        logDebug("Received JSON response, parsing tracks");
        cachedTracks = parseTracksFromJson(jsonResponse);
        tracksCached = true;
        logDebug("Tracks cached successfully, count: " + std::to_string(cachedTracks.size()));
    } else {
        logDebug("Empty JSON response from backend - authentication may have failed");
    }
}
