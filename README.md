# BO3Enhanced

[BO3Enhanced](https://github.com/shiversoftdev/BO3Enhanced/releases/tag/Current) is a mod for the Windows Store edition of Black Ops 3 that allows you to crossplay with Steam and use the Steam game files.

This version of Black Ops 3 has **massively improved performance**, allowing for 200+ fps stable with much faster boot times, and is overall easier to work with for mod developers due to the lack of Arxan DRM. Additionally, this project **completely disables Treyarch Anticheat**, allowing you to debug the game in x64dbg.

Download: https://github.com/shiversoftdev/BO3Enhanced/releases/tag/Current

## Quick Installation

1. Install Steam Black Ops 3
2. Acquire the [Windows Store BO3 Executable and Dependencies](https://www.youtube.com/watch?v=rBZZTcSJ9_s)
3. Place the Windows Store BO3 Executable and Dependencies into the Steam game installation folder (Steam Library->Right Click Black Ops III->Properties->Installed Files->Browse) (Replace all if it asks)
4. Download the [Released Files](https://github.com/shiversoftdev/BO3Enhanced/releases/tag/Current) and place them into the Steam game installation folder (Replace all if it asks)
5. Launch the game through steam. You should see "BO3Enhanced" as the version string if the installation was successful

## Important Info
- No, you will not get banned for this.
- This project only works on Windows (and for the foreseeable future will remain this way).
- This project includes support for the T7Patch linux config file. After first boot you can edit the config for a network password and will be able to play with other T7Patch players if you wish. The name editor requires a reboot.
- This version of the game is still vulnerable to all the remote crashes from the Steam version, but the easy RCEs are not an issue (accidental by Treyarch). I recommend using network password and friends only.
- You cannot use the regular T7Patch with this version of the game (at this time).
- Some workshop maps and mods will not work with this version of the game until they update their native components. You can report an issue with the mod/map here but we will most likely tell you to contact the developer of the mod to convert things on their end.
- This mod will automatically subscribe you to the T7WS Updater workshop mod (which is how we deliver updates for this project).

## Credits
- [Emma / IPG](https://github.com/InvoxiPlayGames) - Project Developer
- [Anthony / serious](https://github.com/shiversoftdev) - Project Developer
- [ssno](https://github.com/ssnob) - TAC research and assistance with several components of the project

## Build From Source
- Requires VS 2022 and Desktop C++ components (v143)
1. `git clone https://github.com/shiversoftdev/BO3Enhanced.git`
2. Acquire the [Steamworks SDK](https://partner.steamgames.com/?goto=%2Fdownloads%2Flist) and copy the 'steam' folder from 'your/sdk/folder/public' into the project directory
3. In `framework.h`, edit the `IS_DEVELOPMENT` macro to be `1` to skip the update checking logic.
4. Build the Release x64 configuration. Output will be in the /bin folder. 
5. **Make sure you rename your `steam_api64.dll` to `steam_api65.dll`**!!
6. You can use the provided `WindowsCodecs.dll` from the [releases tab](https://github.com/shiversoftdev/BO3Enhanced/releases/tag/Current), or if you wish you can use [CFF Explorer](https://ntcore.com/explorer-suite-iii-cff-explorer-vii/) to add an import to your system32/WindowsCodecs.dll -- Import should be by name from `T7WSBootstrapper.dll`, function name `dummy_proc`.
