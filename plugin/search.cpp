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
        const size_t MAX_RESULTS = plugin->getSearchResultLimit();
        
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
                
                // Check if this is a track message
                if (response.find("\"type\":\"track\"") != std::string::npos) {
                    // Parse individual track from the new WebSocket format
                    TrackInfo track;
                    track.size = 0;
                    
                    // Extract fileName from data.fileName
                    size_t fileNameStart = response.find("\"fileName\":");
                    if (fileNameStart != std::string::npos) {
                        fileNameStart = response.find('"', fileNameStart + 11) + 1;
                        size_t fileNameEnd = response.find('"', fileNameStart);
                        if (fileNameEnd != std::string::npos) {
                            track.name = response.substr(fileNameStart, fileNameEnd - fileNameStart);
                        }
                    }
                    
                    // Extract cleanPath for uniqueId from data.cleanPath
                    size_t pathStart = response.find("\"cleanPath\":");
                    if (pathStart != std::string::npos) {
                        pathStart = response.find('"', pathStart + 12) + 1;
                        size_t pathEnd = response.find('"', pathStart);
                        if (pathEnd != std::string::npos) {
                            track.uniqueId = response.substr(pathStart, pathEnd - pathStart);
                            // Use the directory part of cleanPath as directory
                            size_t lastSlash = track.uniqueId.find_last_of('/');
                            if (lastSlash != std::string::npos && lastSlash > 0) {
                                track.directory = track.uniqueId.substr(0, lastSlash);
                            } else {
                                track.directory = "Unknown";
                            }
                        }
                    }
                    
                    // Extract fullUrl from data.fullUrl
                    size_t urlStart = response.find("\"fullUrl\":");
                    if (urlStart != std::string::npos) {
                        urlStart = response.find('"', urlStart + 10) + 1;
                        size_t urlEnd = response.find('"', urlStart);
                        if (urlEnd != std::string::npos) {
                            track.url = response.substr(urlStart, urlEnd - urlStart);
                        }
                    }
                    
                    // Only add track if we have essential fields
                    if (!track.name.empty() && !track.uniqueId.empty() && !track.url.empty()) {
                        allResults.push_back(track);
                        
                        // Add track to VirtualDJ immediately
                        std::string artist = track.directory;
                        if (artist.empty() || artist == "/") {
                            artist = "Unknown Artist";
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
                        
                        logDebug("Added track: " + track.name + " (Total: " + std::to_string(allResults.size()) + "/" + std::to_string(MAX_RESULTS) + ")");

                        // Check if we've reached our limit
                        if (allResults.size() >= MAX_RESULTS) {
                            logDebug("Reached maximum results limit (" + std::to_string(MAX_RESULTS) + "), stopping search");
                            break;
                        }
                    } else {
                        logDebug("Skipping incomplete track data");
                    }
                } else {
                    logDebug("Received non-track message, continuing...");
                }
                
            } catch (const std::exception& e) {
                logDebug("WebSocket read error: " + std::string(e.what()));
                break;
            }
        }

        // Close connection
        ws.close(websocket::close_code::normal);
        
        logDebug("WebSocket connection closed. Returning " + std::to_string(allResults.size()) + " results");
        logDebug("OnSearch completed with " + std::to_string(allResults.size()) + " results");
        return S_OK;

    } catch (const std::exception& e) {
        logDebug("WebSocket connection error: " + std::string(e.what()));
        return S_OK; // Return success but with no results
    }
}
