#include "getFolderList.h"
#include "utilities.h"
#include "../AMP.h"
#include <string>
#include <cstring>

HRESULT getFolderList(CAMP* plugin, IVdjSubfoldersList* subfoldersList) {
    logDebug("GetFolderList called");

    logDebug("Fetching fields from API");
    std::string jsonResponse = plugin->httpGetWithAuth("https://music.abelldjcompany.com/api/fields-db");
    if (jsonResponse.empty()) {
        logDebug("Empty JSON response from fields API");
        return S_OK;
    }

    // Parse fields from JSON response
    logDebug("Parsing fields from JSON response");
    size_t fieldsPos = jsonResponse.find("\"fields\"");
    if (fieldsPos == std::string::npos) {
        logDebug("'fields' not found in JSON response");
        return S_OK;
    }

    size_t arrayStart = jsonResponse.find('[', fieldsPos);
    if (arrayStart == std::string::npos) {
        logDebug("Fields array start '[' not found");
        return S_OK;
    }
    
    // Parse each field object
    size_t pos = arrayStart + 1;
    int fieldCount = 0;
    
    while (pos < jsonResponse.length() && fieldCount < 1000) {
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

        std::string fieldObj = jsonResponse.substr(objStart, objEnd - objStart + 1);

        // Extract field name
        size_t nameStart = fieldObj.find("\"name\":");
        if (nameStart != std::string::npos) {
            nameStart = fieldObj.find('"', nameStart + 7) + 1;
            size_t nameEnd = fieldObj.find('"', nameStart);
            if (nameEnd != std::string::npos) {
                std::string fieldName = fieldObj.substr(nameStart, nameEnd - nameStart);

                // Extract path count for display
                size_t pathCountStart = fieldObj.find("\"pathCount\":");
                std::string displayName = fieldName;
                if (pathCountStart != std::string::npos) {
                    pathCountStart += 12; // Skip "pathCount":
                    size_t pathCountEnd = fieldObj.find_first_of(",}", pathCountStart);
                    if (pathCountEnd != std::string::npos) {
                        std::string pathCountStr = fieldObj.substr(pathCountStart, pathCountEnd - pathCountStart);
                        displayName = fieldName + " (" + pathCountStr + ")";
                    }
                }

                subfoldersList->add(fieldName.c_str(), displayName.c_str());
                fieldCount++;
            }
        }

        pos = objEnd + 1;
    }

    logDebug("GetFolderList completed - added " + std::to_string(fieldCount) + " fields");
    return S_OK;
}
