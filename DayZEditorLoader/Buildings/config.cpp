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
    class ObjectRemoverBase: HouseNoDestruct
    {
        scope=1;
        model="__.p3d";
    };

    class ObjectRemover10x10: ObjectRemoverBase
    {
        radius=10;
    };

    class ObjectRemover20x20: ObjectRemoverBase
    {
        radius=20;
    };

    class ObjectRemover50x50: ObjectRemoverBase
    {
        radius=50;
    };
};