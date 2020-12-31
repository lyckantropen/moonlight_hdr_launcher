# Moonlight HDR Launcher

This is a launcher for GameStream that mocks Mass Effect Andromeda to launch
any executable from a Shield device or
[Moonlight](https://moonlight-stream.org/) with HDR support. For example, it
can be used in conjunction with
[gamestream_launchpad](https://github.com/cgarst/gamestream_launchpad) to
launch GOG Galaxy 2.0 in HDR mode.

It is loosely based on a solution by [tmac666 on NVIDIA forums](https://www.nvidia.com/en-us/geforce/forums/user/213217/338010/hdr-fix-for-gamestream-and-steam-big-picture-mode/).

The installer puts the files in `C:\Program Files\moonlight_hdr_launcher` and
modifies the GeForce Experience entry for Mass Effect Andromeda to launch the
mock executable.

## Installing

Download [latest release here](https://github.com/lyckantropen/moonlight_hdr_launcher/releases/latest/download/install.7z).

Simply launch `installer.exe`. At some point there will be a dialog window
asking you to rescan for games in GeForce Experience. Only then the last part
of the script can succeed. Afterwards, restart GeForce Experience by killing
the processes in Task Manager or simply reboot your PC. Now in Shield or
Moonlight you should see the new entry.

**Note**: I haven't been able to get the cover art to work for me. Maybe you
can get better mileage.

## Description

GeForce Experience recognizes Mass Effect Andromeda as a game that supports HDR
and will initiate a GameStream session with HDR support if it is launched.

This software provides a mock `MassEffectAndromeda.exe` and patches the
NvBackend cache to launch it instead of the game.

The installation does the following:

* Creates `C:\Program Files\moonlight_hdr_launcher` and puts
  `MassEffectAndromeda.exe` there along with an initial configuration file.
* Sets write permissions for all users on `C:\Program Files\moonlight_hdr_launcher`.
* Caches the location `C:\Program Files\moonlight_hdr_launcher` in registry
  (`HKCU/lyckantropen/moonlight_hdr_launcher`).
* Searches `<user folder>\AppData\Local\NVIDIA\NvBackend\StreamingAssetsData\`
  for the `mass_effect_andromeda` folder and patches `StreamingSettings.json`
  and `metadata.json` (the latter only for file checksums).

The `MassEffectAndromeda.exe` launcher does the following:

* Queries the registry for the install location
  (`C:\Program Files\moonlight_hdr_launcher` by default.).
* Forces setting the current directory to the install location.
* Creates the `moonlight_hdr_launcher_log.txt` log file.
* Looks for the `moonlight_hdr_launcher.ini` configuration file.
* Optionally attempts to enable HDR on all connected monitors with HDR support
  if both `options.toggle_hdr` and `options.wait_on_process` are set to non-zero
  (doing so when not waiting for the process to finish would disable HDR
  immenently because of NVAPI unloading).
* Launches the `options.launcher_exe` command as a subprocess and optionally
  waits for it to terminate if `options.wait_on_process` is set to non-zero.
* In the event of an exception, collects crash information and uploads it to
  [sentry.io](https://sentry.io). The data is anonymous, *no unique user or
  machine data is transferred*.

## Configuration

The installer puts a blank `moonlinght_hdr_launcher.ini` file in `C:\Program
Files\moonlight_hdr_launcher`. You can modify it to your needs. It has only 3
options:

* `launcher_exe` - the command to launch
* `wait_on_process` - set to 1 to wait until the `launcher_exe` command completes
* `toggle_hdr` - set to 1 to turn on HDR on all supported monitors for the time
  when `launcher_exe` is running

I **highly** recommend the setup using
[gamestream_launchpad](https://github.com/cgarst/gamestream_launchpad). Simply
put `gamestream_gog_galaxy.ini` and `gamestream_launchpad.exe` in `C:\Program
Files\moonlight_hdr_launcher` and uncomment the relevant lines in
`moonlinght_hdr_launcher.ini`.

### Example configurations

#### HDTV & HDR monitor owners

If you have an HDR monitor connected to your PC or an HDMI dongle that supports
HDR, you can rely on the `toggle_hdr` option.

This example will use
[`gamestream_launchpad`](https://github.com/cgarst/gamestream_launchpad) to
launch GOG Galaxy. First step is to put `gamestream_gog_galaxy.ini` and
`gamestream_launchpad.exe` in `C:\Program Files\moonlight_hdr_launcher`.

`moonlight_hdr_launcher.ini`:
```ini
[options]
launcher_exe = gamestream_launchpad.exe 2560 1440 gamestream_gog_galaxy.ini
toggle_hdr = 1
wait_on_process = 1
```

`gamestream_gog_galaxy.ini`:
```ini
[LAUNCHER]
launcher_path = %%programfiles(x86)%%\GOG Galaxy\GalaxyClient.exe
launcher_window_name = GOG Galaxy 2

[BACKGROUND]

[SETTINGS]
debug = 0
sleep_on_exit = 0
close_watch_method = window
```

#### HDTV owners without HDR monitor

**Note**: This will not make OS-level HDR games (e.g. Cyberpunk 2077) work. Buy
an HDMI dongle with HDR support.

The only way (allegedly) to achieve HDR support is to launch Steam Big Picture
through the launcher. Bear in mind that this solution creates sessions that
need to be force-quit from Moonlight. This example does not use
`gamestream_launchpad`. *Your mileage may vary.*

`moonlight_hdr_launcher.ini`:
```ini
[options]
launcher_exe = C:\Program Files (x86)\Steam\steam.exe steam://open/bigpicture
wait_on_process = 0
# no HDR toggle because there are no HDR monitors and wait_on_process=0
toggle_hdr = 0
```

#### HDTV owners withouth HDR monitor (`gamestream_launchpad` version)

This is the same as the previous example, but using
[`gamestream_launchpad`](https://github.com/cgarst/gamestream_launchpad).

`moonlight_hdr_launcher.ini`:
```ini
[options]
# launch Steam Big Picture using gamestream_launchpad
# please adjust resolution
launcher_exe = gamestream_launchpad.exe 2560 1440 gamestream_steam_bp.ini
wait_on_process = 1
toggle_hdr = 1
```

`gamestream_steam_bp.ini`:
```ini
[LAUNCHER]
launcher_path = %%programfiles(x86)%%\Steam\steam.exe steam://open/bigpicture Fullscreen
# set it to something other than Steam because gamestream_launchpad can detect multiple windows and will close prematurely
launcher_window_name = NotSteam

[BACKGROUND]

[SETTINGS]
debug = 0
sleep_on_exit = 0
close_watch_method = window
```

## FAQ & Troubleshooting

* **I want to be able to play Mass Effect Andromeda while using this launcher.**

  This hasn't been tested, but theoretically you are still able
  to launch Mass Effect Andromeda through the launcher you're
  running, be it GOG Galaxy 2 or Steam.

* **The installer keeps saying that it can't find the entry for Mass Effect Andromeda.**

  * Force-quit GeForce Experience.
  * Remove `C:\Program Files\moonlight_hdr_launcher` or rename
    `MassEffectAndromeda.exe` to something else.
  * Restart GeForce Experience and scan for games.
  * Force-quit GeForce Experience or restart your PC.
  * Re-instate `MassEffectAndromeda.exe` or re-run the installer.
  * Scan for games in GeForce Experience.

* **I don't have an HDR monitor and I want to play Cyberpunk 2077 in HDR.**

  You can't. Cyberpunk 2077 is an OS-level HDR game. You need at least an HDMI
  dongle that supports HDR.

* **Nothing happens when I run the "Mass Effect Andromeda" entry in Moonlight/Shield.**

  Go to `C:\Program Files\moonlight_hdr_launcher` and try running the
  `MassEffectAndromeda.exe` file directly. See if the
  `moonlight_hdr_launcher_log.txt` file is being created. If reporting an
  issue, describe what happened and attach the log file.

  Go to `<user folder>\AppData\Local\NVIDIA\NvBackend\StreamingAssetsData\` and
  see if the `mass_effect_andromeda\<number>\` folder exists and contains
  `StreamingSettings.json`. If either do not exist, see the answer to *The
  installer keeps saying that it can't find the entry for Mass Effect
  Andromeda.*.

  Check the write permissions on `C:\Program Files\moonlight_hdr_launcher`
  (should be write for all users).

* **I'd like to report an error.**
  
  Create an issue in GitHub and attach:
  * `moonlight_hdr_launcher_install.log` (found alongside `install.exe`);
  * `moonlight_hdr_launcher_log.txt` (from `C:\Program Files\moonlight_hdr_launcher`);
  * all `*.ini` files from `C:\Program Files\moonlight_hdr_launcher`;
  * `<user folder>\AppData\Local\NVIDIA\NvBackend\StreamingAssetsData\mass_effect_andromeda\<number>\StreamingSettings.json`;

## Building the launcher executable

* Obtain Boost
* Obtain NVAPI
* Build with CMake, setting `NVAPI_DL_PATH` to the location of the NVAPI zip file

If you don't know how to build projects with CMake, better not attempt it.

## Building the installer

First, install Python 3. Then:

```bash
pip install -r requirements.txt
```

```bash
pyinstaller install.spec
```

`installer.exe` will be put in `dist`.

## Attribution

This code uses parts of [hdr-switch](https://github.com/bradgearon/hdr-switch)
by Brad Gearon under the following license:

> MIT License
> 
> Copyright (c) 2020 Brad Gearon
> 
> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
> 
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
> 
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

This code uses the [WinReg library](https://github.com/GiovanniDicanio/WinReg)
by Giovanni Dicanio under the following license:

> MIT License
> 
> Copyright (c) 2017-2020 by Giovanni Dicanio
> 
> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
> 
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
> 
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.
