@echo off

cd /D "%~dp0"

IF exist "P:\DayZEditorLoader\" (
	echo Removing existing link P:\DayZEditorLoader
	rmdir "P:\DayZEditorLoader\"
)

echo Creating link P:\DayZEditorLoader
mklink /J "P:\DayZEditorLoader\" "%cd%\DayZEditorLoader\"

echo Done