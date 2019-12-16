---
name: Bug report
about: Report crashes, rendering issues etc.
title: ''
labels: ''
assignees: ''

---

Please describe your issue as accurately as possible. If you run into a problem with a binary release, make sure to test with latest `master` as well.

**Important:** When reporting an issue with a specific game or application, such as crashes or rendering issues, please include log files and a D3D11/D3D9 Apitrace (see https://github.com/apitrace/apitrace) so that the issue can be reproduced.
In order to create a trace for **D3D11/D3D10**: Run `wine apitrace.exe trace -a dxgi YOURGAME.exe`.
In order to create a trace for **D3D9**: Follow https://github.com/Joshua-Ashton/d9vk/wiki/Making-a-Trace.
DO NOT use DXVK together with apitrace!

### Software information
Name of the game, settings used etc.

### System information
- GPU:
- Driver:
- Wine version: 
- DXVK version: 

### Apitrace file(s)
- Put a link here

### Log files
- d3d9.log:
- d3d11.log:
- dxgi.log:
