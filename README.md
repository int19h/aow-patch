# Age of Wonders unofficial patch

This *unofficial* patch for Age of Wonders 1.36 tentatively fixes the "Exception during MapViewer.ShowScene" error that can be seen when running AoW on Windows 8 and above, or under Wine on Linux. The error manifests as the following dialog:

![image](https://cloud.githubusercontent.com/assets/1175142/26041050/03a44698-38df-11e7-84cb-21b444c28ea8.png)

This patch does not fix any other errors. If your error does not fit the exact description above, do not expect it to do anything useful.

Applying this patch will result in minor graphical artifacts on the main map, affecting the topmost row of tiles, and any objects that overlap it. It is a known and expected side effect. Do not report it as a bug. 

THIS PATCH IS **UNOFFICIAL**. It is not made by, nor supported by, Truimph Studios. You install it at your own risk.

To install the patch, download the most recent version from [Releases](https://github.com/int19h/aow-patch/releases), open the .zip file, and run the .exe inside. You will need to point the patcher at `Ilpack.dll` inside your AoW directory. It will try to automatically determine the location of that file from registry if possible, but this may not work reliably for Steam and GoG installs. 

Please see this thread on Steam Forums for AoW for more information:
http://steamcommunity.com/app/61500/discussions/0/215439774855125498/?tscn=1494750787
