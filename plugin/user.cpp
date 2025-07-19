#include "../AMP.h"
#include "utilities.h"

HRESULT VDJ_API CAMP::OnLogin()
{
    logDebug("OnLogin called");
    
    return S_FALSE;
}

HRESULT VDJ_API CAMP::OnLogout()
{
    logDebug("OnLogout called");
    tracksCached = false;
    cachedTracks.clear();
    logDebug("Logout completed");
    return S_OK;
}

// Login implementation
HRESULT VDJ_API CAMP::IsLogged()
{
    logDebug("IsLogged called");
    return  S_OK;
}

