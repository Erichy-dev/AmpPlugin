#include "../AMP.h"
#include "utilities.h"

HRESULT VDJ_API CAMP::OnLogin()
{
    logDebug("OnLogin called");
    
    // For now, use the default API key for testing
    // In production, you'd prompt the user for their API key
    std::string testKey = "amp-default-key-123";
    
    if (validateApiKey(testKey)) {
        storeApiKey(testKey);
        apiKey = testKey;
        logDebug("Login successful with test API key");
        return S_OK;
    }
    
    logDebug("Login failed - invalid API key");
    return S_FALSE;
}

HRESULT VDJ_API CAMP::OnLogout()
{
    logDebug("OnLogout called");
    clearApiKey();
    apiKey.clear();
    tracksCached = false;
    cachedTracks.clear();
    logDebug("Logout completed");
    return S_OK;
}

// Login implementation
HRESULT VDJ_API CAMP::IsLogged()
{
    logDebug("IsLogged called");
    apiKey = getStoredApiKey();
    bool logged = !apiKey.empty();
    logDebug("IsLogged result: " + std::string(logged ? "true" : "false"));
    return logged ? S_OK : S_FALSE;
}

