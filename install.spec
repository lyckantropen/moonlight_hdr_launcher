# -*- mode: python ; coding: utf-8 -*-
import os
from pathlib import Path

block_cipher = None
spec_root = os.path.abspath(SPECPATH)
version_str = Path('install/dist/version').read_text()

a = Analysis(['install.py'],
             pathex=[spec_root],
             binaries=[],
             datas=[
                 ('install/dist/mass_effect_andromeda-box-art.jpg', 'dist'),
                 ('install/dist/mass_effect_andromeda-box-art.png', 'dist'),
                 ('install/dist/gamestream_steam_bp.ini', 'dist'),
                 ('install/dist/MassEffectAndromeda.exe', 'dist'),
                 ('install/dist/version', 'dist'),
             ] + ([('install/dist/crashpad_handler.exe', 'dist')] if os.path.exists('install/dist/crashpad_handler.exe') else []),
             hiddenimports=[],
             hookspath=[],
             runtime_hooks=[],
             excludes=[],
             win_no_prefer_redirects=False,
             win_private_assemblies=False,
             cipher=block_cipher,
             noarchive=False)
pyz = PYZ(a.pure, a.zipped_data,
             cipher=block_cipher)
exe = EXE(pyz,
          a.scripts,
          a.binaries,
          a.zipfiles,
          a.datas,
          [],
          name=f'install-{version_str}',
          debug=False,
          bootloader_ignore_signals=False,
          strip=False,
          upx=True,
          upx_exclude=[],
          runtime_tmpdir=None,
          console=True,
          uac_admin=True,
          version='version.txt')