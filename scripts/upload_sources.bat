@ECHO OFF

SET PATH="%PROGRAMFILES%\7-Zip";%PATH%

python -u -B scripts/upload_sources.py %1
