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
	override void InitialiseWorldData()
	{
		super.InitialiseWorldData();
		
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
			Print("Spawning " + data_import.Type);
			EditorSpawnObject(data_import.Type, data_import.Position, data_import.Orientation);
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
}