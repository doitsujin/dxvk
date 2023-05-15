---
name: Bug report
about: Report crashes, rendering issues etc.
title: ''
labels: ''
assignees: ''

---

Please describe your issue as accurately as possible.

If you use Windows, please check the following page: https://github.com/doitsujin/dxvk/wiki/Windows

### Software information
Name of the game, settings used etc.

### System information
- GPU:
- Driver:
- Wine version: 
- DXVK version: 

### Apitrace file(s)
- Put a link here

For instructions on how to use apitrace, see: https://github.com/doitsujin/dxvk/wiki/Using-Apitrace

### Log files
Please attach Proton or Wine logs as a text file:
- When using Proton, set the Steam launch options for your game to `PROTON_LOG=1 %command%` and attach the corresponding `steam-xxxxx.log` file in your home directory.
- When using regular Wine, use `wine game.exe > game.log 2>&1` and attach the resulting `game.log` file.