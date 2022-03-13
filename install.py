import json
import os
import sys
from distutils.errors import DistutilsFileError
from distutils.file_util import copy_file
from hashlib import sha256
from os.path import expandvars
from pathlib import Path
from tkinter import messagebox
from typing import List
from winreg import CreateKey, OpenKey, SetValueEx, CloseKey, KEY_WRITE, HKEY_CURRENT_USER, REG_SZ
import logging

import oschmod

_logger = logging.getLogger('moonlight_hdr_launcher')
LAUNCHER_EXE = 'MassEffectAndromeda.exe'
ADDITIONAL_PROGRAMFILES_FILES = ['moonlight_hdr_launcher.ini', 'crashpad_handler.exe']
ADDITIONAL_STREAMING_FILES = ['mass_effect_andromeda-box-art.png', 'mass_effect_andromeda-box-art.jpg']
REG_PATH = R'SOFTWARE\lyckantropen\moonlight_hdr_launcher'
DESTINATION_FOLDER = R'C:\Program Files\moonlight_hdr_launcher'


def get_source_folder() -> Path:
    try:
        source_folder = Path(sys._MEIPASS)
    except Exception:
        source_folder = Path(__file__).parent.absolute()
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


def write_path_to_reg(destination_folder: Path, reg_path: str, reg_key: str) -> None:
    CreateKey(HKEY_CURRENT_USER, reg_path)
    registry_key = OpenKey(HKEY_CURRENT_USER, reg_path, 0, KEY_WRITE)
    SetValueEx(registry_key, reg_key, 0, REG_SZ, str(destination_folder))
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
        streaming_settings['GameData'][0][
            'StreamingCommandLine'] = f'start {self.launcher_exe}'
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

        # set StreamingAssetsData folder read-write
        oschmod.set_mode_recursive(str(mad_path), 0o777)

        # copy files to StreamingAssetsData destination
        copy_files(self.streaming_files, self.destination_folder, cmdline)

        self.modify_streamsettings_json(mad_path)
        self.modify_metadata_json(mad_path)

        # set StreamingAssetsData folder read-only
        oschmod.set_mode_recursive(str(mad_path), 'a+r,a-w')

        show_warning('The installer will now attempt to restart GeForce Experience. If the new entry does not appear, reboot your PC.', cmdline)
        # kill NVIDIA processes
        os.system('taskkill /f /im "nvcontainer.exe"')
        os.system('taskkill /f /im "NVIDIA Share.exe"')


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--launcher-exe', type=str, required=False, default=LAUNCHER_EXE, help='Name of the launcher')
    parser.add_argument('--destination-folder', type=str, required=False,
                        default=DESTINATION_FOLDER, help='Destination path')
    parser.add_argument('--cmdline', action='store_true', help='Do not show windows with prompts')
    args = parser.parse_args(sys.argv[1:])

    log_file = Path(__file__).parent.absolute() / 'moonlight_hdr_launcher_install.log'
    print(f'Log file: {log_file}')
    logging.basicConfig(format='%(asctime)s:%(levelname)s:%(message)s',
                        datefmt='%m/%d/%Y %I:%M:%S %p',
                        filename=log_file.as_posix(),
                        level=logging.DEBUG)

    launcher_exe = args.launcher_exe
    destination_folder = Path(args.destination_folder)
    cmdline = args.cmdline

    installer = MoonlightHdrLauncherInstaller(
        source_folder=get_source_folder(),
        destination_folder=destination_folder,
        launcher_exe=launcher_exe,
        additional_programfiles_files=ADDITIONAL_PROGRAMFILES_FILES,
        additional_streaming_files=ADDITIONAL_STREAMING_FILES,
        reg_path=REG_PATH)

    installer.install(cmdline)
