class CfgPatches
{
	class Editor_Loader_Buildings
	{
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
		requiredAddons[] = {"DZ_Data"};
	};
};

class CfgVehicles
{
    class HouseNoDestruct;
    class BuildingYeeter20x20: HouseNoDestruct
    {
        scope=1;
        model="DayZEditorLoader\\Buildings\\DebugCylinder.p3d";
    };
};