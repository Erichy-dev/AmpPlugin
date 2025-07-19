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

    std::string encodedSearch = plugin->urlEncode(searchTerm);
    std::string searchUrl = "https://music.abelldjcompany.com/api/tracks?search=" + encodedSearch + "&limit=" + std::to_string(plugin->getSearchResultLimit());
    logDebug("Performing HTTP GET search with URL: " + searchUrl);

    std::string jsonResponse = plugin->httpGet(searchUrl);
    logDebug("Received HTTP response length: " + std::to_string(jsonResponse.length()));

    std::vector<TrackInfo> tracksFound = plugin->parseTracksFromJson(jsonResponse);
    logDebug("Parsed " + std::to_string(tracksFound.size()) + " tracks from JSON response.");

    for (const auto& track : tracksFound) {
        // Parse title and artist from track name
        auto titleArtistPair = parseTrackTitleAndArtist(track.name);
        std::string title = titleArtistPair.first;
        std::string artist = titleArtistPair.second;

        const char* streamUrl = nullptr;
        std::string localPath;
        if (plugin->isTrackCached(track.uniqueId.c_str())) {
            localPath = plugin->getEncodedLocalPathForTrack(track.uniqueId.c_str());
            streamUrl = localPath.c_str();
        }

        bool isVideo = track.name.find(".mp4") != std::string::npos;

        tracks->add(
            track.uniqueId.c_str(), 
            title.c_str(), // title
            artist.c_str(), // artist
            "amp", // remix
            nullptr, // genre
            "AMP", // label
            "AbellDj Music Pool", // comment
            "", // coverUrl
            streamUrl, // sream url
            0, // length
            0, // bpm
            0, // key
            0, // year
            isVideo, 
            false
        );
        logDebug("Added track: " + track.name + " -> Title: " + title + ", Artist: " + artist);
    }
    logDebug("OnSearch completed with " + std::to_string(tracksFound.size()) + " results.");
    return S_OK;
}
