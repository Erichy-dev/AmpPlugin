#ifndef VDJ_SEARCH_H
#define VDJ_SEARCH_H

#include "../vdjPlugin8.h"
#include "../vdjOnlineSource.h"  // This defines IVdjTracksList
#include <string>
#include <vector>

class CAMP; // Forward declaration for the main plugin class
struct TrackInfo; // Forward declaration for the track info struct

// Search function declaration
HRESULT search(CAMP* plugin, const char* searchTerm, IVdjTracksList* tracks);

#endif // VDJ_SEARCH_H
