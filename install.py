import json
import logging
import os
import shutil
import sys
import tempfile
import tkinter as tk
import traceback
from distutils.errors import DistutilsFileError
from distutils.file_util import copy_file
from enum import Enum
from hashlib import sha256
from os.path import expandvars
from pathlib import Path
from tkinter import messagebox
from typing import List, Tuple
from winreg import (HKEY_CURRENT_USER, KEY_READ, KEY_WRITE, REG_SZ, CloseKey,
                    CreateKey, DeleteKey, OpenKey, QueryValueEx, SetValueEx)

import oschmod

_logger = logging.getLogger('moonlight_hdr_launcher')
LAUNCHER_EXE = 'MassEffectAndromeda.exe'
ADDITIONAL_PROGRAMFILES_FILES = ['crashpad_handler.exe', 'gamestream_steam_bp.ini']
ADDITIONAL_STREAMING_FILES = ['mass_effect_andromeda-box-art.png', 'mass_effect_andromeda-box-art.jpg']
REG_PATH = R'SOFTWARE\lyckantropen\moonlight_hdr_launcher'
DESTINATION_FOLDER = R'C:\Program Files\moonlight_hdr_launcher'
GSLP_URL = "https://github.com/cgarst/gamestream_launchpad/releases/download/0.8/gamestream_launchpad.zip"


class BaseConfig(Enum):
    BigPicture = 1
    GogGalaxy = 2
    RemoteDesktop = 3


class ResolutionConfig(Enum):
    Res1080p = 0
    Res1440p = 1
    Res4k = 2
    ResNative = 3


BASE_CONFIGS = {
    BaseConfig.BigPicture: '''
[options]
# launch Steam Big Picture using gamestream_launchpad
launcher_exe = gamestream_launchpad.exe {res_x} {res_y} gamestream_steam_bp.ini
wait_on_process = 1
toggle_hdr = 1
''',
    BaseConfig.GogGalaxy: '''
[options]
launcher_exe = gamestream_launchpad.exe {res_x} {res_y} gamestream_gog_galaxy.ini --no-nv-kill
toggle_hdr = 1
wait_on_process = 1
compatibility_window = 1
''',
    BaseConfig.RemoteDesktop: '''
[options]
remote_desktop = 1
toggle_hdr = 1
wait_on_process = 1
compatibility_window = 1
res_x = {res_x}
res_y = {res_y}
refresh_rate_use_max = 1
'''
}

RESOLUTION_CONFIGS = {
    ResolutionConfig.Res1080p: (1920, 1080),
    ResolutionConfig.Res1440p: (2560, 1440),
    ResolutionConfig.Res4k: (3840, 2160)
}


def install_or_uninstall_select() -> bool:
    root = tk.Tk()
    install = tk.IntVar()
    install.set(1)

    text = tk.Label(root, text="What do you want to do?")
    ri = tk.Radiobutton(root, text="Install", variable=install, value=1)
    ru = tk.Radiobutton(root, text="Uninstall (will remove current config)", variable=install, value=0)
    text.pack(anchor=tk.CENTER)
    ri.pack(anchor=tk.W)
    ru.pack(anchor=tk.W)

    OK = tk.Button(root, text="OK", command=lambda: root.destroy())
    Cancel = tk.Button(root, text="Cancel", command=sys.exit)
    OK.pack(anchor=tk.CENTER)
    Cancel.pack(anchor=tk.CENTER)
    root.mainloop()

    return install.get()


def configuration_select() -> Tuple[BaseConfig, ResolutionConfig]:
    root = tk.Tk()
    cfg = tk.IntVar()
    cfg.set(1)
    res = tk.IntVar()
    res.set(0)

    text1 = tk.Label(root, text="Please select initial configuration")
    text2 = tk.Label(root, text="Launcher:")
    R11 = tk.Radiobutton(root, text="Steam Big Picture", variable=cfg, value=1)
    R12 = tk.Radiobutton(root, text="GOG Galaxy 2.0", variable=cfg, value=2)
    R13 = tk.Radiobutton(root, text="Remote desktop", variable=cfg, value=3)
    text3 = tk.Label(root, text="Resolution:")
    R21 = tk.Radiobutton(root, text="1920x1080 (1080p)", variable=res, value=0)
    R22 = tk.Radiobutton(root, text="2560x1440 (1440p)", variable=res, value=1)
    R23 = tk.Radiobutton(root, text="3840x2160 (4k)", variable=res, value=2)
    R24 = tk.Radiobutton(root, text="Native (detect)", variable=res, value=3)
    text1.pack(anchor=tk.CENTER)
    text2.pack(anchor=tk.CENTER)
    R11.pack(anchor=tk.W)
    R12.pack(anchor=tk.W)
    R13.pack(anchor=tk.W)
    text3.pack(anchor=tk.CENTER)
    R21.pack(anchor=tk.W)
    R22.pack(anchor=tk.W)
    R23.pack(anchor=tk.W)
    R24.pack(anchor=tk.W)

    OK = tk.Button(root, text="OK", command=lambda: root.destroy())
    Cancel = tk.Button(root, text="Cancel", command=sys.exit)
    OK.pack(anchor=tk.CENTER)
    Cancel.pack(anchor=tk.CENTER)

    root.mainloop()
    return BaseConfig(cfg.get()), ResolutionConfig(res.get())


def download_gamestream_launchpad(dest: Path) -> None:
    import io
    import zipfile

    import requests

    r = requests.get(GSLP_URL, stream=True)
    z = zipfile.ZipFile(io.BytesIO(r.content))
    z.extractall(dest)


def get_resolution() -> Tuple[int, int]:
    import ctypes
    user32 = ctypes.windll.user32
    user32.SetProcessDPIAware()
    return user32.GetSystemMetrics(0), user32.GetSystemMetrics(1)


def get_source_folder() -> Path:
    try:
        source_folder = Path(sys._MEIPASS)
    except Exception:
        source_folder = Path(__file__).parent.absolute() / "install"
    return source_folder


def get_sha256(file: Path) -> str:
    content_bytes = file.read_bytes()
    return ''.join([hex(b)[2:] for b in sha256(content_bytes).digest()])


def show_error(message: str, cmdline: bool = False) -> None:
    _logger.error(message)
    if cmdline:
        print(f'ERROR: {message}')
    else:
        messagebox.showerror('Error', message)


def show_warning(message: str, cmdline: bool = False) -> None:
    _logger.warning(message)
    if cmdline:
        print(f'WARNING: {message}')
    else:
        messagebox.showwarning('Warning', message)


def show_info(message: str, cmdline: bool = False) -> None:
    _logger.debug(message)
    if cmdline:
        print(f'INFO: {message}')
    else:
        messagebox.showinfo('Information', message)


def write_path_to_reg(destination_folder: Path, reg_path: str, reg_key: str) -> None:
    CreateKey(HKEY_CURRENT_USER, reg_path)
    registry_key = OpenKey(HKEY_CURRENT_USER, reg_path, 0, KEY_WRITE)
    SetValueEx(registry_key, reg_key, 0, REG_SZ, str(destination_folder))
    CloseKey(registry_key)


def read_path_from_reg(reg_path: str, reg_key: str) -> str:
    registry_key = OpenKey(HKEY_CURRENT_USER, reg_path, 0, KEY_READ)
    value, _ = QueryValueEx(registry_key, reg_key)
    CloseKey(registry_key)
    return value


def delete_key(reg_path: str, reg_key: str) -> None:
    registry_key = OpenKey(HKEY_CURRENT_USER, reg_path, 0, KEY_WRITE)
    DeleteKey(registry_key, reg_key)
    CloseKey(registry_key)


def create_folder(folder: Path, cmdline: bool) -> None:
    try:
        folder.mkdir(parents=True, exist_ok=True)
    except DistutilsFileError as e:
        show_warning(f'No permission to create {folder}, re-run as Administrator', cmdline)
        raise e


def copy_files(source_paths: List[Path], destination_folder: Path, cmdline: bool) -> None:
    for source_path in source_paths:
        if source_path.exists():
            try:
                dest_name, copied = copy_file(str(source_path), str(destination_folder), update=True)
                if copied:
                    _logger.info(f'Copied {source_path} to {dest_name}')
                else:
                    _logger.info(f'Skipped copying {source_path} to {dest_name} because destination is newer than source')
            except DistutilsFileError as e:
                show_warning(f'No permission to copy {source_path} to {destination_folder}, re-run as Administrator', cmdline)
                raise e
        else:
            _logger.warning(f'Source file {source_path} does not exist')


def get_masseffectandromeda_location(cmdline: bool) -> Path:
    # find StreamingAssetsData subfolder
    try:
        app_data = Path(expandvars(r'%LOCALAPPDATA%'))
        _logger.info(f'Found AppData path: {app_data}')
        mad_path = next(app_data.glob(
            '**/StreamingAssetsData/mass_effect_andromeda/*'))
        _logger.info(f'Found StreamingAssetsData folder for Mass Effect Andromeda: {mad_path}')
        return mad_path
    except StopIteration as e:
        show_error('Unable to find entry for Mass Effect Andromeda. Have you tried scanning for games in GeForce Experience?', cmdline)
        raise e


class MoonlightHdrLauncherInstaller:
    def __init__(self, source_folder: Path,
                 destination_folder: Path,
                 launcher_exe: str,
                 additional_programfiles_files: List[str],
                 additional_streaming_files: List[str],
                 reg_path: str):
        self.source_folder = source_folder
        self.destination_folder = destination_folder
        self.launcher_exe = launcher_exe
        self.additional_programfiles_files = additional_programfiles_files
        self.additional_streaming_files = additional_streaming_files
        self.reg_path = reg_path
        self.launcher_path = Path(source_folder / 'dist' / launcher_exe)
        self.programfiles_files = [self.launcher_path] + [source_folder / 'dist' / file_path for file_path in additional_programfiles_files]
        self.streaming_files = [source_folder / 'dist' / file_path for file_path in additional_streaming_files]

    def modify_streamsettings_json(self, mad_path: Path) -> None:
        streaming_settings_path = mad_path / 'StreamingSettings.json'
        streaming_settings = json.loads(streaming_settings_path.read_text())
        streaming_settings['GameData'][0]['StreamingDisplayName'] = 'HDR Launcher'
        streaming_settings['GameData'][0]['StreamingCaption'] = 'HDR Launcher'
        streaming_settings['GameData'][0]['StreamingClassName'] = 'HDR Launcher'
        streaming_settings['GameData'][0]['StreamingCommandLine'] = f'start {self.launcher_exe}'
        final_json = json.dumps(streaming_settings, indent=4)
        _logger.debug(f'Final StreamingSettings.json: {final_json}')
        _logger.debug(f'Saving to {streaming_settings_path}')
        streaming_settings_path.write_text(final_json)

    def modify_metadata_json(self, mad_path: Path) -> None:
        streaming_settings_path = mad_path / 'StreamingSettings.json'
        metadata_path = mad_path / 'metadata.json'
        metadata = json.loads(metadata_path.read_text())
        metadata['metadata']['files'] = [{
            'filename': f.name,
            'url': '',
            'sha256': get_sha256(f), 'size': f.stat().st_size} for f in [streaming_settings_path, *self.streaming_files]
        ]
        final_metadata_json = json.dumps(metadata, indent=4)
        _logger.debug(f'Final metadata.json: {final_metadata_json}')
        _logger.debug(f'Saving to {metadata_path}')
        metadata_path.write_text(final_metadata_json)

    def remove_from_applicationdata_json(self, mad_path: Path) -> None:
        applicationdata_path = mad_path.parent.parent / 'ApplicationData.json'
        applicationdata = json.loads(applicationdata_path.read_text())
        applicationdata['metadata'].pop('mass_effect_andromeda')
        applicationdata_path.write_text(json.dumps(applicationdata, indent=4))

    def install(self, cmdline: bool) -> None:
        _logger.debug(f'Installing Moonlight HDR Launcher {(self.source_folder / "dist" / "version").read_text()}')
        _logger.debug(f'Source folder: {self.source_folder}')
        _logger.debug(f'Destination folder for launcher: {self.destination_folder}')
        _logger.debug(f'Launcher path: {self.launcher_path}')
        _logger.debug(f'List of files to put in {self.destination_folder}: {self.programfiles_files}')
        _logger.debug(f'List of files to put in Streaming folder: {self.streaming_files}')

        if not self.launcher_path.exists():
            show_error(f'{self.launcher_path} does not exist', cmdline)
            raise Exception(f'{self.launcher_path} does not exist')

        create_folder(self.destination_folder, cmdline)
        copy_files(self.programfiles_files, self.destination_folder, cmdline)

        # install gamestream launchpad
        try:
            with tempfile.TemporaryDirectory() as gslp_temp:
                _logger.debug(f'Downloading gamestream_launchpad files from: {GSLP_URL}')
                download_gamestream_launchpad(Path(gslp_temp))
                gslp_files = list(Path(gslp_temp).glob('**/*'))
                _logger.debug(f'Downloaded gamestream_launchpad files: {gslp_files}')
                # install
                copy_files(gslp_files, self.destination_folder, cmdline)
        except Exception as e:
            _logger.error(f'Error downloading gamestream_launchpad: {e}')
            show_error(f'Error downloading gamestream_launchpad: {e}', cmdline)

        # install initial configuration
        ini_file = self.destination_folder / 'moonlight_hdr_launcher.ini'
        if not ini_file.exists():
            config, res = configuration_select()
            _logger.debug(f'Selected base configuration: {str(config)}')

            if res == ResolutionConfig.ResNative:
                try:
                    res_x, res_y = get_resolution()
                except BaseException:
                    res_x, res_y = 1920, 1080
            else:
                res_x, res_y = RESOLUTION_CONFIGS[res]
            _logger.debug(f'Writing config file: {ini_file}')
            config_ini = BASE_CONFIGS[config].format(res_x=res_x, res_y=res_y)
            ini_file.write_text(config_ini)
            _logger.debug(f'Written configuration: {config_ini}')

        # set destination folder read-write
        oschmod.set_mode_recursive(str(self.destination_folder), 0o777)

        # write destination_folder to registry
        try:
            _logger.debug(f'Writing destination_folder="{self.destination_folder}" to registry at "{self.reg_path}"')
            write_path_to_reg(self.destination_folder, self.reg_path, 'destination_folder')
        except WindowsError as e:
            show_error(f'Failed to write destination_folder to registry: {e}')
            raise e

        show_warning('Before clicking OK, open GeForce Experience and re-scan for games.', cmdline)

        mad_path = get_masseffectandromeda_location(cmdline)
        write_path_to_reg(mad_path, self.reg_path, 'stream_assets_folder')

        # set StreamingAssetsData folder read-write
        oschmod.set_mode_recursive(str(mad_path), 0o777)

        # copy files to StreamingAssetsData destination
        copy_files(self.streaming_files, mad_path, cmdline)

        self.modify_streamsettings_json(mad_path)
        self.modify_metadata_json(mad_path)

        # set StreamingAssetsData folder read-only
        oschmod.set_mode_recursive(str(mad_path), 'a+r,a-w')

        show_warning('The installer will now attempt to restart GeForce Experience. If the new entry does not appear, reboot your PC.', cmdline)
        # kill NVIDIA processes
        os.system('taskkill /f /im "nvcontainer.exe"')
        os.system('taskkill /f /im "NVIDIA Share.exe"')

    def uninstall(self, cmdline: bool) -> None:
        try:
            _logger.debug('Uninstalling Moonlight HDR Launcher')
            # mad_path = get_masseffectandromeda_location(cmdline)
            destination_folder = Path(read_path_from_reg(self.reg_path, 'destination_folder'))
            stream_assets_folder = Path(read_path_from_reg(self.reg_path, 'stream_assets_folder'))

            _logger.debug(f'Read destination_folder from registry: {destination_folder}, exists={destination_folder.exists()}')
            _logger.debug(f'Read stream_assets_folder from registry: {stream_assets_folder}, exists={stream_assets_folder.exists()}')

            if stream_assets_folder.exists():
                _logger.debug(f'Setting {stream_assets_folder.parent} back to read-write')
                oschmod.set_mode_recursive(str(stream_assets_folder.parent), 0o777)
                _logger.debug(f'Removing {stream_assets_folder.parent} recursively')
                shutil.rmtree(stream_assets_folder.parent)
                self.remove_from_applicationdata_json(stream_assets_folder)

            if destination_folder.exists():
                _logger.debug(f'Setting {destination_folder} back to read-write')
                oschmod.set_mode_recursive(str(destination_folder), 0o777)
                _logger.debug(f'Removing {destination_folder} recursively')
                shutil.rmtree(destination_folder)

            _logger.debug(f'Removing destination_folder from {self.reg_path}')
            delete_key(self.reg_path, 'destination_folder')
            _logger.debug(f'Removing stream_assets_folder from {self.reg_path}')
            delete_key(self.reg_path, 'stream_assets_folder')

            show_warning(
                'The uninstaller will now attempt to restart GeForce Experience.'
                'Go to GeForce Experience and re-scan for games. If the entry does not disappear, reboot your PC.',
                cmdline)
            # kill NVIDIA processes
            os.system('taskkill /f /im "nvcontainer.exe"')
            os.system('taskkill /f /im "NVIDIA Share.exe"')
        except Exception as e:
            _logger.error(e)
            _logger.error(traceback.format_exc())
            show_error(f'Failed to uninstall: {e}')
            raise e


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--launcher-exe', type=str, required=False, default=LAUNCHER_EXE, help='Name of the launcher')
    parser.add_argument('--destination-folder', type=str, required=False,
                        default=DESTINATION_FOLDER, help='Destination path')
    parser.add_argument('--cmdline', action='store_true', help='Do not show windows with prompts')
    parser.add_argument('--uninstall', action='store_true', help='Uninstall Moonlight HDR Launcher')
    args = parser.parse_args(sys.argv[1:])

    log_file = Path(__file__).parent.absolute() / 'moonlight_hdr_launcher_install.log'
    print(f'Log file: {log_file}')
    logging.basicConfig(format='%(asctime)s:%(levelname)s:%(message)s',
                        datefmt='%m/%d/%Y %I:%M:%S %p',
                        filename=log_file.as_posix(),
                        level=logging.DEBUG)
    file_output_handler = logging.FileHandler('moonlight_hdr_launcher_install_log.txt')
    _logger.addHandler(file_output_handler)

    launcher_exe = args.launcher_exe
    destination_folder = Path(args.destination_folder)
    cmdline = args.cmdline

    install_or_uninstall = False if args.uninstall else install_or_uninstall_select()

    installer = MoonlightHdrLauncherInstaller(
        source_folder=get_source_folder(),
        destination_folder=destination_folder,
        launcher_exe=launcher_exe,
        additional_programfiles_files=ADDITIONAL_PROGRAMFILES_FILES,
        additional_streaming_files=ADDITIONAL_STREAMING_FILES,
        reg_path=REG_PATH)

    if install_or_uninstall:
        installer.install(cmdline)
    else:
        try:
            installer.uninstall(cmdline)
            show_info('Uninstalled successfully', cmdline)
        except Exception:
            show_error('Uninstallation failed', cmdline)
