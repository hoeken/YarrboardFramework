# v3.0.1

A small maintenance release with an HTTPS redirect server, a dependency bump, and a frontend refactor.

## ✨ New Features

### HTTPS / SSL
- When SSL is enabled, a second plaintext server now listens on port 80 and redirects all requests to the HTTPS endpoint, so visiting `http://` no longer fails

## 🔧 Improvements

### Frontend & UI
- Full config handlers (save, copy, download, and shareable export) refactored to jQuery style for brevity

### Dependencies
- Bumped `PsychicHttp` to `^3.1.0`