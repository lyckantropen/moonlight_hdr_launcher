import json
import sys
from distutils.errors import DistutilsFileError
from distutils.file_util import copy_file
from hashlib import sha256
from os.path import expandvars
from pathlib import Path
from tkinter import messagebox
from typing import List


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
    if cmdline:
        print(f'ERROR: {message}')
    else:
        messagebox.showerror('Error', message)


def show_warning(message: str, cmdline: bool = False) -> None:
    if cmdline:
        print(f'WARNING: {message}')
    else:
        messagebox.showwarning('Warning', message)


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
            copy_file(source_path, destination_folder, update=True)
            print(f'Copied {source_path} to {destination_folder}')
        except DistutilsFileError as e:
            show_warning(f'No permission to copy {source_path} to {destination_folder}, re-run as Administrator', cmdline)
            raise e

    # prompt to re-scan games
    show_warning(
        'Before continuing, open GeForce Experience and re-scan for games.', cmdline)

    # find StreamingAssetsData subfolder
    try:
        app_data = Path(expandvars(r'%LOCALAPPDATA%'))
        mad_path = next(app_data.glob(
            '**/StreamingAssetsData/mass_effect_andromeda/*'))
    except StopIteration:
        show_error('Unable to find entry for Mass Effect Andromeda. Have you tried scanning for games in GeForce Experience?', cmdline)

    # copy files to StreamingAssetsData destination
    for source_file in streaming_files:
        try:
            copy_file(source_file, mad_path, update=True)
            print(f'Copied {source_file} to {mad_path}')
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
    print(f'Final JSON: {final_json}')
    print(f'Saving to {streaming_settings_path}')
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
    print(f'Final JSON: {final_metadata_json}')
    print(f'Saving to {metadata_path}')
    metadata_path.write_text(final_metadata_json)

    show_warning('Now restart GeForce Experience or the PC to register the config changes', cmdline)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--launcher-exe', type=str, required=False, default='MassEffectAndromeda.exe', help='Name of the launcher')
    parser.add_argument('--destination-folder', type=str, required=False,
                        default=R'C:\Program Files\moonlight_hdr_launcher', help='Destination path')
    parser.add_argument('--cmdline', action='store_true', help='Do not show windows with prompts')
    args = parser.parse_args(sys.argv[1:])

    launcher_exe = args.launcher_exe
    destination_folder = Path(args.destination_folder)
    cmdline = args.cmdline

    install_launcher(source_folder=get_source_folder(),
                     destination_folder=destination_folder,
                     launcher_exe=launcher_exe,
                     additional_programfiles_files=['moonlight_hdr_launcher.ini'],
                     additional_streaming_files=['mass_effect_andromeda-box-art.png', 'mass_effect_andromeda-box-art.jpg'],
                     cmdline=cmdline)
