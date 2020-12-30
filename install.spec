# -*- mode: python ; coding: utf-8 -*-
import os

block_cipher = None
spec_root = os.path.abspath(SPECPATH)

a = Analysis(['install.py'],
             pathex=[spec_root],
             binaries=[],
             datas=[
                 ('dist/mass_effect_andromeda-box-art.jpg', 'dist'),
                 ('dist/mass_effect_andromeda-box-art.png', 'dist'),
                 ('dist/moonlight_hdr_launcher.ini', 'dist'),
                 ('dist/MassEffectAndromeda.exe', 'dist'),
                 ('dist/version', 'dist'),
                 ('dist/crashpad_handler.exe', 'dist')
             ],
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
          name='install',
          debug=False,
          bootloader_ignore_signals=False,
          strip=False,
          upx=True,
          upx_exclude=[],
          runtime_tmpdir=None,
          console=True,
          uac_admin=True,
          version='version.txt')