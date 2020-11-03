
class CfgPatches
{
	class Editor_Loader_Scripts
	{
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
		requiredAddons[] = {"DZ_Scripts"};
	};
};

class CfgMods 
{
	class DZ_Editor_Loader
	{
		name = "DayZ Editor Loader";
		dir = "DayZEditorLoader";
		credits = "InclementDab";
		author = "InclementDab";
		creditsJson = "DayZEditorLoader/Scripts/Data/Credits.json";
		versionPath = "DayZEditorLoader/scripts/Data/Version.hpp";
		inputs = "DayZEditorLoader/Scripts/Data/Inputs.xml";
		type = "mod";
		dependencies[] =
		{
			"Game", "World", "Mission"
		};
		class defs
		{
			class imageSets
			{
				files[]=
				{
					"DayZEditorLoader/gui/imagesets/dayz_editor_gui.imageset"
				};
			};
			class engineScriptModule
			{
				value = "";
				files[] =
				{
					"DayZEditorLoader/scripts/common",
					"DayZEditorLoader/scripts/1_core"
				};
			};

			class gameScriptModule
			{
				value="";
				files[] = 
				{
					"DayZEditorLoader/scripts/common",
					"DayZEditorLoader/scripts/3_Game"
				};
			};
			class worldScriptModule
			{
				value="";
				files[] = 
				{
					"DayZEditorLoader/scripts/common",
					"DayZEditorLoader/scripts/4_World"
				};
			};

			class missionScriptModule 
			{
				value="";
				files[] = 
				{
					"DayZEditorLoader/scripts/common",
					"DayZEditorLoader/scripts/5_Mission"
				};
			};
		};
	};
};
