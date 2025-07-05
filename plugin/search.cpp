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
    std::string searchUrl = "https://music.abelldjcompany.com/api/tracks?search=" + encodedSearch + "&limit=" + std::to_string(plugin->getSearchResultLimit());
    logDebug("Performing HTTP GET search with URL: " + searchUrl);

    std::string jsonResponse = plugin->httpGetWithAuth(searchUrl);
    logDebug("Received HTTP response length: " + std::to_string(jsonResponse.length()));

    std::vector<TrackInfo> tracksFound = plugin->parseTracksFromJson(jsonResponse);
    logDebug("Parsed " + std::to_string(tracksFound.size()) + " tracks from JSON response.");

    for (const auto& track : tracksFound) {
        std::string artist = track.directory;
        if (artist.empty() || artist == "/") {
            artist = "";
        }

        const char* streamUrl = nullptr;
        std::string localPath;
        if (plugin->isTrackCached(track.uniqueId.c_str())) {
            localPath = plugin->getEncodedLocalPathForTrack(track.uniqueId.c_str());
            streamUrl = localPath.c_str();
        }

        bool isVideo = track.name.find(".mp4") != std::string::npos;

        tracks->add(
            track.uniqueId.c_str(), track.name.c_str(), artist.c_str(),
            "", nullptr, "Music Pool", "", "", streamUrl,
            0, 0, 0, 0, isVideo, false
        );
        logDebug("Added track: " + track.name);
    }
    logDebug("OnSearch completed with " + std::to_string(tracksFound.size()) + " results.");
    return S_OK;
}
