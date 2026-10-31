@echo off
rem ---- zhiwushibie plant recognition one-click launcher ----
rem deps: Qt 6.10.1 msvc2022_64 + OpenCV world 4.12 (vc16)
cd /d "D:\installed\Plants_Camera\zhiwushibie\build_fresh\Debug"
set "PATH=D:\installed\Qt\6.10.1\msvc2022_64\bin;D:\installed\opencv\build\x64\vc16\bin;%PATH%"
start "" zhiwushibie.exe
