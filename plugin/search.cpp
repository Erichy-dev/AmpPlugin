#include "search.h"
#include "utilities.h"
#include "../AMP.h"
#include <string>
#include <cstring>

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <iostream>
#include <cstdlib>

namespace beast = boost::beast;             // from <boost/beast.hpp>
namespace http = beast::http;               // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;     // from <boost/beast/websocket.hpp>
namespace net = boost::asio;                // from <boost/asio.hpp>
namespace ssl = net::ssl;                   // from <boost/asio/ssl.hpp>
using tcp = net::ip::tcp;                   // from <boost/asio/ip/tcp.hpp>

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
    std::string host = "music.abelldjcompany.com";
    std::string port = "443";
    std::string path = "/ws/search?apiKey=amp-1750760187709-i2roomisf";

    logDebug("Performing WebSocket search with host: " + host + ", path: " + path);

    try {
        // Setup WebSocket connection
        net::io_context ioc;
        ssl::context ctx(ssl::context::tlsv12_client);
        ctx.set_default_verify_paths();

        websocket::stream<beast::ssl_stream<tcp::socket>> ws(ioc, ctx);

        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(host, port);

        // Connect TCP
        net::connect(ws.next_layer().next_layer(), results.begin(), results.end());

        // TLS handshake
        ws.next_layer().handshake(ssl::stream_base::client);

        // WebSocket handshake
        ws.handshake(host, path);

        // Send search query
        std::string searchMessage = "{\"q\":\"" + encodedSearch + "\"}";
        logDebug("Sending search message: " + searchMessage);
        ws.write(net::buffer(searchMessage));

        // Read streaming results
        beast::flat_buffer buffer;
        std::vector<TrackInfo> allResults;
        
        while (true) {
            try {
                buffer.clear();
                ws.read(buffer);
                
                std::string response = beast::buffers_to_string(buffer.data());
                logDebug("Received WebSocket message: " + response);
                
                // Check if this is an end-of-stream message
                if (response.find("\"type\":\"end\"") != std::string::npos) {
                    logDebug("Received end-of-stream message");
                    break;
                }
                
                // Parse tracks from this chunk of the response
                std::vector<TrackInfo> chunkResults = plugin->parseTracksFromJson(response);
                
                // Add to our total results
                allResults.insert(allResults.end(), chunkResults.begin(), chunkResults.end());
                
                // Optionally, you could add tracks immediately as they arrive:
                // for (const auto& track : chunkResults) {
                //     // Add track processing code here if you want real-time updates
                // }
                
            } catch (const std::exception& e) {
                logDebug("WebSocket read error: " + std::string(e.what()));
                break;
            }
        }

        // Close connection
        ws.close(websocket::close_code::normal);
        
        logDebug("WebSocket connection closed. Total results: " + std::to_string(allResults.size()));

        // Process all collected results
        if (allResults.empty()) {
            logDebug("No tracks found from WebSocket search.");
            return S_OK;
        }

        logDebug("Adding " + std::to_string(allResults.size()) + " search results");
        for (const auto& track : allResults) {
            std::string artist = track.directory;
            if (artist.empty() || artist == "/") {
                artist = "Unknown Artist";
            }

            const char* streamUrl = nullptr;
            std::string localPath;
            if (plugin->isTrackCached(track.uniqueId.c_str())) {
                localPath = plugin->getEncodedLocalPathForTrack(track.uniqueId.c_str());
                streamUrl = localPath.c_str();
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

        logDebug("OnSearch completed with " + std::to_string(allResults.size()) + " results");
        return S_OK;

    } catch (const std::exception& e) {
        logDebug("WebSocket connection error: " + std::string(e.what()));
        return S_OK; // Return success but with no results
    }
}
