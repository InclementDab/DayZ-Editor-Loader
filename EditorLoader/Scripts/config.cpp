
class CfgPatches
{
	class EditorLoader_Scripts
	{
        requiredVersion = 0.1;
		requiredAddons[] = {"DZ_Scripts"};
	};
};

class CfgMods 
{
	class EditorLoader
	{
		name = "DayZ-Mod-Template";
		dir = "EditorLoader";
		creditsJson = "EditorLoader/Scripts/Credits.json";
		inputs = "EditorLoader/Scripts/Inputs.xml";
		type = "mod";

		dependencies[] =
		{
			"Game", "World", "Mission"
		};
		class defs
		{
			class imageSets
			{
				files[]= {};
			};
			class engineScriptModule
			{
				files[] =
				{
					"EditorLoader/scripts/1_core"
				};
			};

			class gameScriptModule
			{
				files[] = 
				{
					"EditorLoader/scripts/3_Game"
				};
			};
			class worldScriptModule
			{
				files[] = 
				{
					"EditorLoader/scripts/4_World"
				};
			};

			class missionScriptModule 
			{
				files[] = 
				{
					"EditorLoader/scripts/5_Mission"
				};
			};
		};
	};
};
