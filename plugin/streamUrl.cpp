#include "streamUrl.h"
#include "utilities.h"
#include "../AMP.h"
#include <string>
#include <cstring>
#include <thread>


HRESULT getStreamUrl(CAMP* plugin, const char* uniqueId, IVdjString& url, IVdjString& errorMessage) {
    std::string id = uniqueId ? uniqueId : "(null)";
    
    // Call onstream endpoint in a separate thread to avoid blocking
    std::thread([plugin, id]() {
        std::string onstreamUrl = "https://music.abelldjcompany.com/api/fields/most-played/tracks";
        std::string postData = "{\"cleanPath\": \"" + id + "\"}";
        plugin->httpPost(onstreamUrl, postData);
    }).detach();

    logDebug("GetStreamUrl called with uniqueId: '" + id + "'");
    
    // First, check if the track is cached locally
    if (plugin->isTrackCached(uniqueId)) {
        std::string localPath = plugin->getEncodedLocalPathForTrack(uniqueId);
        logDebug("Track is cached. Returning local path: " + localPath);
        url = localPath.c_str();
        return S_OK;
    }
    
    // If not cached, look for the track in our full track list to get the remote URL
    logDebug("Track not cached. Searching in memory...");
    plugin->ensureTracksAreCached();
    for (const auto& track : plugin->cachedTracks) {
        if (track.uniqueId == id) {
            logDebug("Found track in memory: " + track.url);
            url = track.url.c_str();
            return S_OK;
        }
    }
    
    // If not found in cache or memory, construct URL directly as a fallback with proper URL encoding
    if (!id.empty() && id != "fallback") {
        std::string encodedPath = plugin->urlEncode(id);
        std::string streamUrl = "https://music.abelldjcompany.com/audio/" + encodedPath;
        logDebug("Constructed fallback stream URL: " + streamUrl);
        url = streamUrl.c_str();
        return S_OK;
    }
    
    logDebug("Track not found, returning error");
    errorMessage = "Track not found";
    return S_FALSE;
}
