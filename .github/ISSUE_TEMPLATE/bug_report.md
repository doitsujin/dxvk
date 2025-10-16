---
name: Bug report
about: Report crashes, rendering issues etc.
title: ''
labels: ''
assignees: ''

---

Please describe your issue as accurately as possible. Include screenshots or videos when relevant.

If you use Windows, please check the following page: https://github.com/doitsujin/dxvk/wiki/Windows

### Software information
Name of the game, settings used etc. Include any mods or add-ons if applicable.

### System information
- GPU:
- Driver:
- Wine version: 
- D7VK version: 

### Log files
Please attach Proton or Wine logs as a text file:
- Have `D7VK_LOG_LEVEL` set to `debug`.
- When using Proton, set the Steam launch options for your game to `PROTON_LOG=1 %command%` and attach the corresponding `steam-xxxxx.log` file in your home directory.
- When using regular Wine, use `wine game.exe > game.log 2>&1` and attach the resulting `game.log` file.
- On Windows, D7VK log files will usually appear next to the games exe or in its base folder.
- In case of huge log files, which aren't unexpected, please archive the log file first, upload it and share the download link.
