# Yarrboard Firmware Web Installer

This directory contains the standalone ESP Web Tools firmware installer for Yarrboard devices.

## Usage

Visit the live installer at: https://hoeken.github.io/YarrboardFramework/web/firmware.html

The installer allows you to flash firmware directly to your Yarrboard device from your browser via USB.

## Browser Requirements

The firmware installer requires a browser with Web Serial API support:
- Google Chrome
- Microsoft Edge
- Opera
- Other Chromium-based browsers

**Note**: Firefox and Safari do not currently support the Web Serial API and cannot be used with this installer.

## Features

- **Board Selection**: Choose your specific Yarrboard hardware revision
- **Version Selection**: Select from available firmware versions
- **Changelog Display**: View release notes for each version
- **One-Click Installation**: Flash firmware directly from your browser
- **Dark Mode**: Automatic dark mode support with manual toggle
- **Responsive Design**: Works on desktop, tablet, and mobile devices

## Local Development

To test the installer locally:

1. Open `firmware.html` directly in a supported browser
2. An internet connection is required for:
   - Bootstrap CSS (CDN)
   - marked.js for markdown parsing (CDN)
   - ESP Web Tools (CDN)
   - Firmware manifest from GitHub

## How It Works

1. The page fetches the firmware manifest from GitHub
2. Users select their board model and desired firmware version
3. The manifest contains URLs to ESP Web Tools manifest files
4. ESP Web Tools handles the actual flashing process via Web Serial API

## Files

- `firmware.html` - Main installer page
- `logo.png` - Yarrboard logo
- `README.md` - This file

## GitHub Pages Deployment

This directory is configured to be served via GitHub Pages. Any changes pushed to the main branch will be automatically deployed.
