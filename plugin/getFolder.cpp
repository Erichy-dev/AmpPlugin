#include "getFolder.h"
#include "utilities.h"
#include "../AMP.h"
#include <cstring>
#include <string>

HRESULT getFolder(CAMP* plugin, const char* folderUniqueId, IVdjTracksList* tracksList) {
    std::string folderId = folderUniqueId ? folderUniqueId : "(null)";
    logDebug("GetFolder called with folderUniqueId: '" + folderId + "'");
    
    
    // Fetch tracks for specific field from new API endpoint
    std::string encodedFolderId = plugin->urlEncode(folderId);
    std::string apiUrl = "https://music.abelldjcompany.com/api/fields/" + encodedFolderId + "/tracks";
    logDebug("Fetching tracks from: " + apiUrl);
    std::string jsonResponse = plugin->httpGetWithAuth(apiUrl);
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

        std::string fileName, fullUrl, cleanPath;

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
            std::string localPath;
            if (plugin->isTrackCached(cleanPath.c_str())) {
                localPath = plugin->getEncodedLocalPathForTrack(cleanPath.c_str());
                streamUrl = localPath.c_str();
                logDebug("Track is cached. Returning local path");
                plugin->cb->SendCommand("browsed_file_color \"#00FF00\"");
            }else {
                logDebug("Track is not cached. Returning remote path");
            }

            // check whether the track is a video (mp3 vs mp4)
            bool isVideo = false;
            if (fileName.find(".mp4") != std::string::npos) {
                isVideo = true;
            }
            
            tracksList->add(
                cleanPath.c_str(),        // uniqueId (cleanPath)
                fileName.c_str(),         // title (fileName)
                "",         // artist (field name)
                "",                       // remix
                nullptr,                  // genre
                "",             // label
                "",          // comment
                "",                       // cover URL
                streamUrl,                // streamUrl
                0,                        // length (determined when loaded)
                0,                        // bpm
                0,                        // key
                0,                        // year
                isVideo,                    // isVideo
                false                     // isKaraoke
            );
            trackCount++;
        }

        pos = objEnd + 1;
    }
    
    logDebug("GetFolder completed, added " + std::to_string(trackCount) + " tracks to folder '" + folderId + "'");
    return S_OK;
}
