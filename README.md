# Kontakt 8 Library Removal Tool (K8Tool)

K8Tool (*formerly K8-LRT*) is a Windows tool for removing libraries from the Bobdule version of Kontakt 8. It's written in C++20 and built on [XenUI](https://github.com/jakerieger/XenUI).

![](Docs/screenshot.png)

K8Tool completely removes Kontakt libraries from the cache on disk. Native Instruments stores cache files in several different locations and this tool ensures they're all checked and removed.

Locations that need to be searched and deleted:

- `HKEY_LOCAL_MACHINE\SOFTWARE\Native Instruments\<Library Name>`
- `HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Native Instruments\<Library Name>` (edge-case, still needs checked)
- `C:\Program Files\Common Files\Native Instruments\Service Center\<Library Name>.xml`
- `~\AppData\Local\Native Instruments\Kontakt 8\LibrariesCache\<filename>`
- `~\AppData\Local\Native Instruments\Kontakt 8\komplete.db3`
- `C:\Users\Public\Documents\Native Instruments\Native Access\ras3\<filename>.jwt`
- `C:\Program Files\Common Files\Native Instruments\Kontakt 8\PAResources` [?]

## Download

The latest version of K8Tool is v3.0.0 - you can [download it here](https://github.com/jakerieger/K8Tool/releases/latest).

## Changelog

### 3.0.0 (November 18, 2026)

Massive rewrite in C++, utilizing [XenUI](https://github.com/jakerieger/XenUI).

#### Added
- XenUI library dependency (XenUI.dll)
- Completely new look and feel
- New threading architecture
- Multi-select libraries for removal

#### Fixed
- Memory leaks throughout code base, largely surrounding strings
- Dead/placeholder UI elements such as the "Remove..." button for batch-removal.
- Relocating didn't delete the original library folder, it just copied it. Now relocations and removals completely delete the content folder from disk (no recycle bin).

### 2.0.0 (January 31, 2026)

The 2.0 release of K8Tool brings a lot of bug fixes and reworks a large portion of the code base to improve both reliability and maintainability.

#### Added
- String pools for more robust memory management
- The ability to relocate libraries
- Full NTFS-length path support

#### Fixed
- Crashing if an incorrect value for 'ContentDir' is found in the registry. Now it should simply ignore the content directory when removing the library (and disallow relocating since it doesn't have a location on disk).

#### Removed
- Support for Windows 7

### 1.1.0 (January 26, 2026)

#### Added
- Removes additional files associated with the library cache (thanks to Bobdule for the info)
- Removal dialogs for confirming options prior to removing one or multiple libraries

#### Changed
- Condensed "File" and "Help" menus into a single menu
- Label text changes

#### Fixed
- Bug with checkbox values not persisting

### 1.0.0 (January 25, 2026)

The official 1.0 release of K8Tool.

#### Added
- Update checking
- Log viewer
- Manually reload libraries

#### Changed
- Updated UI look to modern Windows

#### Fixed
- Memory leak constructing path strings
- Inconsistent naming
- Small bug fixes with file I/O

### 0.3.1 (January 23, 2026)

- Improved memory management model
- Added backups for cache files

### 0.3.0 (January 23, 2026)

- Sweeping code changes and refactoring
- Bug fixes and improvements
- Logging for debugging issues if any arise
- Library entry filtering to remove non-library entries
- More robust error handling

### 0.2.0 (January 23, 2026)

- Tons of bug fixes and improvements
- Added a `Remove All` button in place of the quit button
- Filter registry entries to remove non-library entries
- General code improvements

### 0.1.0 (January 22, 2026)

- Initial release of K8Tool
 
## License

K8Tool is licensed under the [Apache 2.0](LICENSE) license.