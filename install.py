import json
from os.path import expandvars
from pathlib import Path
from shutil import copy, copyfile
from tkinter import messagebox
from hashlib import sha256


def get_sha256(file: Path) -> str:
    content_bytes = file.read_bytes()
    return ''.join([hex(b)[2:] for b in sha256(content_bytes).digest()])


launcher_exe = R'MassEffectAndromeda.exe'

pwd = Path(__file__).parent.absolute()

boxart_png = Path(pwd / 'dist' / 'mass_effect_andromeda-box-art.png')
boxart_jpg = Path(pwd / 'dist' / 'mass_effect_andromeda-box-art.jpg')
launcher_path = Path(pwd / 'dist' / launcher_exe)
folder_path = Path(R'C:\Program Files\moonlight_hdr_launcher')
launcher_final_path = folder_path / launcher_exe

if not launcher_path.exists():
    messagebox.showerror('Error', f'{launcher_path} does not exist')
    raise Exception(f'{launcher_path} does not exist')

# create folder in Program Files and copy files
try:
    folder_path.mkdir(parents=True, exist_ok=True)
    copyfile(launcher_path, launcher_final_path)
    copyfile(boxart_jpg, folder_path)
    copyfile(boxart_png, folder_path)
except PermissionError as e:
    messagebox.showerror(
        'Permission Error', f'No permission to copy {launcher_exe} to {launcher_final_path}, re-run as Administrator')
    raise e

# prompt to re-scan games
messagebox.showwarning(
    'Action Required', 'Before continuing, open GeForce Experience and re-scan for games.')

# modify config files
try:
    app_data = Path(expandvars(r'%LOCALAPPDATA%'))
    mad_path = next(app_data.glob(
        '**/StreamingAssetsData/mass_effect_andromeda/*'))
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

    metadata_path = mad_path / 'metadata.json'
    metadata = json.loads(metadata_path.read_text())
    metadata['metadata']['files'] = [{
        'filename': f.name,
        'url': '',
        'sha256': get_sha256(f), 'size': f.stat().st_size} for f in [streaming_settings_path, boxart_png, boxart_jpg]]
    final_metadata_json = json.dumps(metadata, indent=4)
    print(f'Final JSON: {final_metadata_json}')
    print(f'Saving to {metadata_path}')
    metadata_path.write_text(final_metadata_json)
except StopIteration:
    messagebox.showerror(
        'Error', 'Unable to find entry for Mass Effect Andromeda. Have you tried scanning for games in GeForce Experience?')

messagebox.showwarning(
    'Action Required', 'Now restart GeForce Experience or the PC to register the config changes')
