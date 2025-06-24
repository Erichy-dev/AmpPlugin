#include "search.h"
#include "utilities.h"
#include "../AMP.h"
#include <string>
#include <cstring>

HRESULT search(CAMP* plugin, const char* searchTerm, IVdjTracksList* tracks) {
    logDebug("OnSearch called with search term: " + std::string(searchTerm ? searchTerm : "(null)"));

    if (!searchTerm || strlen(searchTerm) == 0) {
        logDebug("Empty search term, returning empty results");
        return S_OK;
    }

    if (plugin->apiKey.empty()) {
        plugin->apiKey = plugin->getStoredApiKey();
    }
    if (plugin->apiKey.empty()) {
        logDebug("User not logged in - no API key available for search.");
        return S_OK;
    }

    std::string encodedSearch = plugin->urlEncode(searchTerm);
    std::string searchUrl = "https://music.abelldjcompany.com/api/search?q=" + encodedSearch;

    logDebug("Performing search with URL: " + searchUrl);

    std::string jsonResponse = plugin->httpGetWithAuth(searchUrl);
    if (jsonResponse.empty()) {
        logDebug("Empty JSON response from search API");
        return S_OK;
    }

    // Parse tracks from JSON response
    logDebug("Parsing search results from JSON response");
    std::vector<TrackInfo> searchResults = plugin->parseTracksFromJson(jsonResponse);
    if (searchResults.empty()) {
        logDebug("No tracks found from search API.");
        return S_OK;
    }

    logDebug("Adding " + std::to_string(searchResults.size()) + " search results");
    for (const auto& track : searchResults) {
        std::string artist = track.directory;
        if (artist.empty() || artist == "/") {
            artist = "Unknown Artist";
        }

        const char* streamUrl = nullptr;
        std::string localPath;
        if (plugin->isTrackCached(track.uniqueId.c_str())) {
            localPath = plugin->getEncodedLocalPathForTrack(track.uniqueId.c_str());
            streamUrl = localPath.c_str();
            plugin->cb->SendCommand("browsed_file_color \"#00FF00\"");
            logDebug("Track is cached. Returning local path");
        } else {
            logDebug("Track is not cached. Returning remote path");
        }

        // check whether the track is a video (mp3 vs mp4)
        bool isVideo = false;
        if (track.name.find(".mp4") != std::string::npos) {
            isVideo = true;
        }

        tracks->add(
            track.uniqueId.c_str(),   // uniqueId
            track.name.c_str(),       // title
            artist.c_str(),           // artist
            "",                       // remix
            nullptr,                  // genre
            "Music Pool",             // label
            "",                       // comment
            "",                       // cover URL
            streamUrl,                // streamUrl
            0,                        // length
            0,                        // bpm
            0,                        // key
            0,                        // year
            isVideo,                  // isVideo
            false                     // isKaraoke
        );
    }

    logDebug("OnSearch completed with " + std::to_string(searchResults.size()) + " results");
    return S_OK;
}
