import json
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


def write_destination_folder_to_reg(destination_folder: Path, reg_path: str = R'SOFTWARE\lyckantropen\moonlight_hdr_launcher') -> None:
    _logger.debug(f'Writing destination_folder="{destination_folder}" to registry at "{reg_path}"')
    CreateKey(HKEY_CURRENT_USER, reg_path)
    registry_key = OpenKey(HKEY_CURRENT_USER, reg_path, 0, KEY_WRITE)
    SetValueEx(registry_key, 'destination_folder', 0, REG_SZ, str(destination_folder))
    CloseKey(registry_key)


def install_launcher(source_folder: Path,
                     destination_folder: Path,
                     launcher_exe: str,
                     additional_programfiles_files: List[str],
                     additional_streaming_files: List[str],
                     cmdline: bool
                     ) -> None:
    launcher_path = Path(source_folder / 'dist' / launcher_exe)
    programfiles_files = [launcher_path] + [source_folder/'dist' / file_path for file_path in additional_programfiles_files]
    streaming_files = [source_folder/'dist' / file_path for file_path in additional_streaming_files]

    _logger.debug(f'Installing Moonlight HDR Launcher {(source_folder/"dist"/"version").read_text()}')
    _logger.debug(f'Source folder: {source_folder}')
    _logger.debug(f'Destination folder for launcher: {destination_folder}')
    _logger.debug(f'Launcher path: {launcher_path}')
    _logger.debug(f'List of files to put in {destination_folder}: {programfiles_files}')
    _logger.debug(f'List of files to put in Streaming folder: {streaming_files}')

    if not launcher_path.exists():
        show_error(f'{launcher_path} does not exist', cmdline)
        raise Exception(f'{launcher_path} does not exist')

    # create destination folder
    try:
        destination_folder.mkdir(parents=True, exist_ok=True)
    except DistutilsFileError as e:
        show_warning(f'No permission to create {destination_folder}, re-run as Administrator', cmdline)
        raise e

    # copy files to programfiles destination
    for source_path in programfiles_files:
        try:
            dest_name, copied = copy_file(source_path, destination_folder, update=True)
            if copied:
                _logger.info(f'Copied {source_path} to {dest_name}')
            else:
                _logger.info(f'Skipped copying {source_path} to {dest_name} because destination is newer than source')
        except DistutilsFileError as e:
            show_warning(f'No permission to copy {source_path} to {destination_folder}, re-run as Administrator', cmdline)
            raise e

    # set destination folder read-write
    oschmod.set_mode_recursive(str(destination_folder), 0o777)

    # write destination_folder to registry
    try:
        write_destination_folder_to_reg(destination_folder)
    except WindowsError as e:
        show_error(f'Failed to write destination_folder to registry: {e}')
        raise e

    # prompt to re-scan games
    show_warning(
        'Before continuing, open GeForce Experience and re-scan for games.', cmdline)

    # find StreamingAssetsData subfolder
    try:
        app_data = Path(expandvars(r'%LOCALAPPDATA%'))
        _logger.info(f'Found AppData path: {app_data}')
        mad_path = next(app_data.glob(
            '**/StreamingAssetsData/mass_effect_andromeda/*'))
        _logger.info(f'Found StreamingAssetsData folder for Mass Effect Andromeda: {mad_path}')
    except StopIteration as e:
        show_error('Unable to find entry for Mass Effect Andromeda. Have you tried scanning for games in GeForce Experience?', cmdline)
        raise e

    # set folder read-write
    oschmod.set_mode_recursive(str(mad_path), 0o777)

    # copy files to StreamingAssetsData destination
    for source_file in streaming_files:
        try:
            dest_name, copied = copy_file(source_file, mad_path, update=True)
            if copied:
                _logger.info(f'Copied {source_file} to {dest_name}')
            else:
                _logger.info(f'Skipped copying {source_file} to {dest_name} because destination is newer than source')
        except DistutilsFileError:
            show_warning(
                f'No permission to copy {source_file} to {mad_path}, re-run as Administrator', cmdline)

    # modify StreamingSettings.json
    streaming_settings_path = mad_path / 'StreamingSettings.json'
    streaming_settings = json.loads(streaming_settings_path.read_text())
    streaming_settings['GameData'][0]['StreamingDisplayName'] = 'HDR Launcher'
    streaming_settings['GameData'][0]['StreamingCaption'] = 'HDR Launcher'
    streaming_settings['GameData'][0]['StreamingClassName'] = 'HDR Launcher'
    streaming_settings['GameData'][0][
        'StreamingCommandLine'] = f'start {launcher_exe}'
    final_json = json.dumps(streaming_settings, indent=4)
    _logger.debug(f'Final StreamingSettings.json: {final_json}')
    _logger.debug(f'Saving to {streaming_settings_path}')
    streaming_settings_path.write_text(final_json)

    # modify metadata.json
    metadata_path = mad_path / 'metadata.json'
    metadata = json.loads(metadata_path.read_text())
    metadata['metadata']['files'] = [{
        'filename': f.name,
        'url': '',
        'sha256': get_sha256(f), 'size': f.stat().st_size} for f in [streaming_settings_path, *streaming_files]
    ]
    final_metadata_json = json.dumps(metadata, indent=4)
    _logger.debug(f'Final metadata.json: {final_metadata_json}')
    _logger.debug(f'Saving to {metadata_path}')
    metadata_path.write_text(final_metadata_json)

    # set folder read-only
    oschmod.set_mode_recursive(mad_path.as_posix(), 'a+r,a-w')

    show_warning('Now restart GeForce Experience or the PC to register the config changes', cmdline)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--launcher-exe', type=str, required=False, default='MassEffectAndromeda.exe', help='Name of the launcher')
    parser.add_argument('--destination-folder', type=str, required=False,
                        default=R'C:\Program Files\moonlight_hdr_launcher', help='Destination path')
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

    install_launcher(source_folder=get_source_folder(),
                     destination_folder=destination_folder,
                     launcher_exe=launcher_exe,
                     additional_programfiles_files=['moonlight_hdr_launcher.ini', 'hdr_toggle.exe'],
                     additional_streaming_files=['mass_effect_andromeda-box-art.png', 'mass_effect_andromeda-box-art.jpg'],
                     cmdline=cmdline)
