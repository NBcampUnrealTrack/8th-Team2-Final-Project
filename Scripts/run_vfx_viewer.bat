@echo off
rem Battle VFX viewer (see run_vfx_viewer.ps1). Put a desktop shortcut to this file.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_vfx_viewer.ps1" %*
