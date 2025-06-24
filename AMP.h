#ifndef AMP_H
#define AMP_H

// we include stdio.h only to use the sprintf() function
// we define _CRT_SECURE_NO_WARNINGS for the warnings of the sprintf() function
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string>
#include <vector>

#include "vdjOnlineSource.h"
#include "plugin/search.h"
#include "plugin/streamUrl.h"
#include "plugin/getFolderList.h"

// Simple structure to hold track information
struct TrackInfo {
    std::string uniqueId;
    std::string name;
    std::string directory;
    std::string url;
    int size;
};

// Forward declare the search function so we can friend it.
HRESULT search(class CAMP* plugin, const char* searchTerm, class IVdjTracksList* tracks);
HRESULT getStreamUrl(class CAMP* plugin, const char* uniqueId, class IVdjString& url, class IVdjString& errorMessage);
HRESULT getFolderList(class CAMP* plugin, class IVdjSubfoldersList* subfoldersList);

class CAMP : public IVdjPluginOnlineSource
{
public:
    // Friend declaration for our search function
    friend HRESULT search(CAMP* plugin, const char* searchTerm, IVdjTracksList* tracks);
    friend HRESULT getStreamUrl(CAMP* plugin, const char* uniqueId, IVdjString& url, IVdjString& errorMessage);
    friend HRESULT getFolderList(CAMP* plugin, IVdjSubfoldersList* subfoldersList);

    HRESULT VDJ_API OnGetPluginInfo(TVdjPluginInfo8* infos) override;
    
    // Login methods
    HRESULT VDJ_API IsLogged() override;
    HRESULT VDJ_API OnLogin() override;
    HRESULT VDJ_API OnLogout() override;
    
    HRESULT VDJ_API OnSearch(const char* search, IVdjTracksList* tracksList) override;
    HRESULT VDJ_API OnSearchCancel() override;
    HRESULT VDJ_API GetStreamUrl(const char* uniqueId, IVdjString& url, IVdjString& errorMessage) override;
    HRESULT VDJ_API GetFolderList(IVdjSubfoldersList* subfoldersList) override;
    HRESULT VDJ_API GetFolder(const char* folderUniqueId, IVdjTracksList* tracksList) override;
    HRESULT VDJ_API GetContextMenu(const char* uniqueId, IVdjContextMenu* contextMenu) override;
    HRESULT VDJ_API OnContextMenu(const char* uniqueId, size_t menuIndex) override;
    HRESULT VDJ_API GetFolderContextMenu(const char *folderUniqueId, IVdjContextMenu *contextMenu) override;
    HRESULT VDJ_API OnFolderContextMenu(const char* folderUniqueId, size_t menuIndex) override;

private:
    // Caching
    void ensureTracksAreCached();
    void downloadTrackToCache(const char* uniqueId);
    void deleteTrackFromCache(const char* uniqueId);
    bool isTrackCached(const char* uniqueId);
    std::string getCacheDir();
    std::string getCachePathForTrack(const char* uniqueId);
    std::string getEncodedLocalPathForTrack(const char* uniqueId);

    // HTTP and JSON parsing functions
    std::string httpGet(const std::string& url);
    std::string httpGetWithAuth(const std::string& url);
    void httpPost(const std::string& url, const std::string& postData);
    bool downloadFile(const std::string& url, const std::string& filePath, const std::string& apiKey);
    std::vector<TrackInfo> parseTracksFromJson(const std::string& jsonString);
    std::string urlEncode(const std::string& value);
    
    // Authentication
    bool validateApiKey(const std::string& apiKey);
    std::string getStoredApiKey();
    void storeApiKey(const std::string& apiKey);
    void clearApiKey();
    
    std::vector<TrackInfo> cachedTracks;
    bool tracksCached = false;
    std::string apiKey;
};

#endif
