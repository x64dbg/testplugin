@echo off
set PLUGINDIR=..\x64dbg\bin\x64\plugins
mkdir %PLUGINDIR%
copy bin\x64\testplugin.dll %PLUGINDIR%\testplugin.dp64