@echo off
cd %TEMP%
curl.exe "https://tts-auth.dreamtonics.com/api/update/fetch?product=VoicepeakEditor&platform=windows" > vpmeta.tmp
set /p data=<vpmeta.tmp
for /f tokens^=4^ delims^=^" %%A in ("%data%") do curl %%A -o vpmeta.exe
set __COMPAT_LAYER=RUNASINVOKER
start vpmeta.exe /currentuser
