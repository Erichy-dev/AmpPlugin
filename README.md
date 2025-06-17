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
