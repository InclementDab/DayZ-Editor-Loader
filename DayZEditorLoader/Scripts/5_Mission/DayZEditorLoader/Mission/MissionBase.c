class EditorObjectDataImport
{
	int m_Id;
	string Type;
	string DisplayName;
	vector Position;
	vector Orientation;
	float Scale;
	int Flags;
}
	
class EditorWorldDataImport
{
	string MapName;
	vector CameraPosition;
	ref array<ref EditorObjectDataImport> EditorObjects = {};
	ref array<int> DeletedObjects = {};
}

modded class MissionBase
{
	protected ref map<int, Object> m_WorldObjects = new map<int, Object>();

	override void InitialiseWorldData()
	{
		super.InitialiseWorldData();
		
		if (!GetGame().IsServer()) return;
		
		if (!FileExist("$profile:/EditorFiles")) {
			Print("EditorFiles directory not found, creating...");
			if (!MakeDirectory("$profile:/EditorFiles")) {
				Print("Could not create EditorFiles directory. Exiting...");
				return;
			}
		}
		
		TStringArray files = {};
		string file_name;
		FileAttr file_attr;
		
		FindFileHandle find_handle = FindFile("$profile:/EditorFiles/*.dze", file_name, file_attr, FindFileFlags.ALL);
		files.Insert(file_name);
		
		while (FindNextFile(find_handle, file_name, file_attr)) {
			files.Insert(file_name);
		}
		
		CloseFindFile(find_handle);
		

		// Adds all map objects to the m_WorldObjects array
		ref array<Object> objects = {};
		ref array<CargoBase> cargos = {};
		GetGame().GetObjectsAtPosition(Vector(7500, 0, 7500), 20000, objects, cargos);

		foreach (Object o: objects) {
			m_WorldObjects.Insert(o.GetID(), o);
		}

		foreach (string file: files) {
			Print("File found: " + file);
			EditorWorldDataImport data_import;
			JsonFileLoader<EditorWorldDataImport>.JsonLoadFile("$profile:/EditorFiles/" + file, data_import);
			
			if (data_import) {
				Print("Loading $profile:/EditorFiles/" + file);
				CreateData(data_import);
			}
		}

	}
	
	private static void CreateData(EditorWorldDataImport editor_data)
	{
		foreach (EditorObjectDataImport data_import: editor_data.EditorObjects) {
			EditorSpawnObject(data_import.Type, data_import.Position, data_import.Orientation);
		}

		foreach (int deleted_object: editor_data.DeletedObjects) {
			EditorDeleteObject(m_WorldObjects[deleted_object]);
		}
	}
	
	private static void EditorSpawnObject(string type, vector position, vector orientation)
	{
	    auto obj = GetGame().CreateObjectEx(type, position, ECE_SETUP | ECE_UPDATEPATHGRAPH | ECE_CREATEPHYSICS);
	    obj.SetPosition(position);
	    obj.SetOrientation(orientation);
	    obj.SetOrientation(obj.GetOrientation());
	    obj.SetFlags(EntityFlags.STATIC, false);
	    obj.Update();
		obj.SetAffectPathgraph(true, false);
		if (obj.CanAffectPathgraph()) GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(GetGame().UpdatePathgraphRegionByObject, 100, false, obj);
	}

	private static void EditorDeleteObject(Object obj)
	{
		
	}
}
