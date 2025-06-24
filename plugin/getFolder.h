#ifndef VDJ_GETFOLDER_H
#define VDJ_GETFOLDER_H

#include "../vdjPlugin8.h"
#include "../vdjOnlineSource.h"

class CAMP;

HRESULT getFolder(CAMP* plugin, const char* folderUniqueId, IVdjTracksList* tracksList);

#endif // VDJ_GETFOLDER_H
