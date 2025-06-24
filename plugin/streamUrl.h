#ifndef VDJ_STREAMURL_H
#define VDJ_STREAMURL_H

#include "../vdjPlugin8.h"
#include "../vdjOnlineSource.h"

class CAMP; // Forward declaration

// The function needs the plugin context and the original parameters
HRESULT getStreamUrl(CAMP* plugin, const char* uniqueId, IVdjString& url, IVdjString& errorMessage);

#endif
