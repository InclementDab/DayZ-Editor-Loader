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
			EditorLoaderLog("EditorFiles directory not found, creating...");
			if (!MakeDirectory("$profile:/EditorFiles")) {
				EditorLoaderLog("Could not create EditorFiles directory. Exiting...");
				return;
			}
		}
		
		EditorLoaderLog("Loading World Objects into cache...");
		
		// Adds all map objects to the m_WorldObjects array
		ref array<Object> objects = {};
		ref array<CargoBase> cargos = {};
		GetGame().GetObjectsAtPosition(Vector(7500, 0, 7500), 20000, objects, cargos);
		
		EditorLoaderLog(string.Format("Loaded %1 World Objects into cache", m_WorldObjects.Count()));
		foreach (Object o: objects) {
			m_WorldObjects.Insert(o.GetID(), o);
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

		foreach (string file: files) {
			EditorLoaderLog("File found: " + file);
			EditorWorldDataImport data_import;
			JsonFileLoader<EditorWorldDataImport>.JsonLoadFile("$profile:/EditorFiles/" + file, data_import);
			
			if (data_import) {
				EditorLoaderLog("Loading $profile:/EditorFiles/" + file);
				EditorLoaderCreateData(data_import);
			}
		}

	}
	
	private void EditorLoaderCreateData(EditorWorldDataImport editor_data)
	{
		EditorLoaderLog(string.Format("%1 created objects found", editor_data.EditorObjects.Count()));
		EditorLoaderLog(string.Format("%1 deleted objects found", editor_data.DeletedObjects.Count()));
		foreach (EditorObjectDataImport data_import: editor_data.EditorObjects) {
			EditorLoaderSpawnObject(data_import.Type, data_import.Position, data_import.Orientation);
		}

		foreach (int deleted_object: editor_data.DeletedObjects) {
			EditorLoaderDeleteObject(m_WorldObjects[deleted_object]);
		}
	}
	
	private static void EditorLoaderSpawnObject(string type, vector position, vector orientation)
	{
		EditorLoaderLog(string.Format("Creating %1", type));
	    auto obj = GetGame().CreateObjectEx(type, position, ECE_SETUP | ECE_UPDATEPATHGRAPH | ECE_CREATEPHYSICS);
	    obj.SetPosition(position);
	    obj.SetOrientation(orientation);
	    obj.SetOrientation(obj.GetOrientation());
	    obj.SetFlags(EntityFlags.STATIC, false);
	    obj.Update();
		obj.SetAffectPathgraph(true, false);
		if (obj.CanAffectPathgraph()) GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(GetGame().UpdatePathgraphRegionByObject, 100, false, obj);
	}

	private static void EditorLoaderDeleteObject(Object obj)
	{
		EditorLoaderLog(string.Format("Deleting %1", obj));
		CF__ObjectManager.RemoveObject(obj);
	}
	
	static void EditorLoaderLog(string msg)
	{
		PrintFormat("[EditorLoader] %1", msg);
	}
}
