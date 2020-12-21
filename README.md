# Moonlight HDR Launcher

This is a launcher for GameStream that mocks Mass Effect Andromeda to launch
any executable from a Shield device or
[Moonlight](https://moonlight-stream.org/) with HDR support. For example, it
can be used in conjunction with
[gamestream_launchpad](https://github.com/cgarst/gamestream_launchpad) to
launch GOG Galaxy 2.0 in HDR mode.

The installer puts the files in `C:\Program Files\moonlight_hdr_launcher` and
modifies the GeForce Experience entry for Mass Effect Andromeda to launch the
mock executable.

## Installing

Simply launch `installer.exe`. At some point there will be a dialog window
asking you to rescan for games in GeForce Experience. Only then the last part
of the script can succeed. Afterwards, restart GeForce Experience by killing
the processes in Task Manager or simply reboot your PC. Now in Shield or
Moonlight you should see the new entry.

**Note**: I haven't been able to get the cover art to work for me. Maybe you
can get better mileage.

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