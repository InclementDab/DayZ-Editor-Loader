
class CfgPatches
{
	class EditorLoader_Scripts
	{
        requiredVersion = 0.1;
		requiredAddons[] = {"DF_Scripts"};
	};
};

class CfgMods 
{
	class EditorLoader
	{
		name = "Editor Loader";
		dir = "EditorLoader";
		creditsJson = "EditorLoader/Scripts/Credits.json";
		inputs = "EditorLoader/Scripts/Inputs.xml";
		type = "mod";

		dependencies[] =
		{
			"Game", "Mission"
		};
		class defs
		{

			class gameScriptModule
			{
				files[] = 
				{
					"EditorLoader/scripts/3_Game"
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
