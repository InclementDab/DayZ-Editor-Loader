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
    class ObjectRemover20x20: HouseNoDestruct
    {
        scope=1;
        model="__.p3d";
    };
};