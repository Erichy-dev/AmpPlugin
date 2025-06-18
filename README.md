# VirtualDJ AMP Plugin

VirtualDJ plugin for streaming music from AMP backend.

## Setup

1. **Generate API Key**: Visit `/amp-admin` on your backend
2. **Save API Key**: Create file with your key:
   - **macOS**: `$HOME/Library/Application Support/VirtualDJ/.camp_session_cache`
   - **Windows**: `%USERPROFILE%\AppData\Local\VirtualDJ\.camp_session_cache`
3. **Login**: Use VirtualDJ's login button for the AMP plugin
4. **Browse**: Access music via "Music" folder

## Backend

- **Endpoint**: `https://music.abelldjcompany.com`
- **Authentication**: API key via `x-api-key` header
- **Default Key**: `amp-default-key-123` (for testing)

## Usage

1. Login to plugin in VirtualDJ
2. Browse music in "Music" folder
3. Load tracks to decks

## Highlighting Cached Tracks

To make it easy to see which tracks are downloaded and available offline, you can set up a "Color Rule" in VirtualDJ. This is a one-time setup that will automatically color any track you've cached from AMP.

1.  **Open Settings**: Click the **gear icon** (⚙️) in the top bar to open the VirtualDJ Settings.
2.  **Go to Browser Settings**: On the left, select the **Browser** tab.
3.  **Find Color Rules**: In the main panel, look for the **Color Rules** section and click on it.
4.  **Create the New Rule**:
    *   In the first box (the condition), type: `comment contains "(Cached)"`
    *   In the second box (the color), select the color you want (e.g., **Green**).
5.  **Save and Close**: Simply close the Settings window.

From now on, any track from the AMP music pool that is cached to your computer will automatically appear in green (or your chosen color) in the browser list.

## Debug

Plugin logs are written to:
- **macOS**: `$HOME/Library/Application Support/VirtualDJ/debug.log`
- **Windows**: `%USERPROFILE%\AppData\Local\VirtualDJ\debug.log`

## Essential Commands

```bash
# monitor logs
> ~/Library/Application\ Support/VirtualDJ/debug.log && tail -f ~/Library/Application\ Support/VirtualDJ/debug.log

# build plgin build/
rm -rf * && cmake .. && make
```
