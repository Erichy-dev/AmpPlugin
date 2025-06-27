#include "AMP.h"
#define FTS_FUZZY_MATCH_IMPLEMENTATION
#include "fts_fuzzy_match.h"
#include "plugin/search.h"
#include "plugin/utilities.h"
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
// These are now in internet.cpp
// #include <windows.h>
// #include <wininet.h>
// #pragma comment(lib, "wininet.lib")
#elif defined(VDJ_MAC)
// These are now in internet.cpp and cache.cpp
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <netdb.h>
// #include <unistd.h>
// #include <sys/stat.h>
#endif

using namespace std;

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

HRESULT VDJ_API CAMP::OnSearch(const char* search, IVdjTracksList* tracksList)
{
    return ::search(this, search, tracksList);
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
    return ::getStreamUrl(this, uniqueId, url, errorMessage);
}

HRESULT VDJ_API CAMP::GetFolderList(IVdjSubfoldersList* subfoldersList)
{
    // return ::getFolderList(this, subfoldersList);
    return S_OK;
}

HRESULT VDJ_API CAMP::GetFolder(const char* folderUniqueId, IVdjTracksList* tracksList)
{
    return ::getFolder(this, folderUniqueId, tracksList);
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
            cb->SendCommand("browsed_file_color \"#85e692\"");
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
    
        int currentLimit = getSearchResultLimit();
        
        string checkmark = " ✓";
        contextMenu->add(("Search Results: 25" + string(currentLimit == 25 ? checkmark : "")).c_str());
        contextMenu->add(("Search Results: 50" + string(currentLimit == 50 ? checkmark : "")).c_str());
        contextMenu->add(("Search Results: 100" + string(currentLimit == 100 ? checkmark : "")).c_str());
        contextMenu->add(("Search Results: 200" + string(currentLimit == 200 ? checkmark : "")).c_str());
    
    logDebug("GetFolderContextMenu completed");
    return S_OK;
}

HRESULT VDJ_API CAMP::OnFolderContextMenu(const char *folderUniqueId, size_t menuIndex)
{
    string folderId = folderUniqueId ? folderUniqueId : "(null)";
    logDebug("OnFolderContextMenu called with folderUniqueId: '" + folderId + "', menuIndex: " + to_string(menuIndex));
    
        int newLimit = 50; // Default
        
        switch (menuIndex) {
            case 0: newLimit = 25; break;
            case 1: newLimit = 50; break;
            case 2: newLimit = 100; break;
            case 3: newLimit = 200; break;
            default: 
                logDebug("Unknown menu index: " + to_string(menuIndex));
                return S_OK;
        }
        
        setSearchResultLimit(newLimit);
        logDebug("Search result limit set to: " + to_string(newLimit));
    
    logDebug("OnFolderContextMenu completed");
    return S_OK;
}


