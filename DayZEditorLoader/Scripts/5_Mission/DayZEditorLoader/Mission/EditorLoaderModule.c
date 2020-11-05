class EditorLoaderModule: JMModuleBase
{
	static ref map<int, Object> WorldObjects = new map<int, Object>();
	protected ref array<ref EditorWorldDataImport> m_WorldDataImports = {};

	override void OnMissionStart()
	{
		GetRPCManager().AddRPC("EditorLoaderModule", "EditorLoaderRemoteCreateData", this);
		
		if (!FileExist("$profile:/EditorFiles")) {
			EditorLoaderLog("EditorFiles directory not found, creating...");
			if (!MakeDirectory("$profile:/EditorFiles")) {
				EditorLoaderLog("Could not create EditorFiles directory. Exiting...");
				return;
			}
		}
		
		EditorLoaderLog("Loading World Objects into cache...");
		
		// Adds all map objects to the WorldObjects array
		ref array<Object> objects = {};
		ref array<CargoBase> cargos = {};
		GetGame().GetObjectsAtPosition(Vector(7500, 0, 7500), 20000, objects, cargos);
		
		foreach (Object o: objects) {
			WorldObjects.Insert(o.GetID(), o);
		}
		
		EditorLoaderLog(string.Format("Loaded %1 World Objects into cache", WorldObjects.Count()));
		
		// Everything below this line is the Server side syncronization :)
		if (!IsMissionHost()) return;
		
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
				m_WorldDataImports.Insert(data_import);
				EditorLoaderLog("Loaded $profile:/EditorFiles/" + file);
			}
		}
		
		foreach (EditorWorldDataImport data: m_WorldDataImports) {
			EditorLoaderCreateData(data);
		}
	}
	
	static void EditorLoaderRemoteCreateData(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
	{		
		EditorLoaderLog("EditorLoaderRemoteCreateData");
		Param1<ref EditorWorldDataImport> data_import;
		if (!ctx.Read(data_import)) {
			return;
		}
		
		EditorLoaderCreateData(data_import.param1);
	}
	
	static void EditorLoaderCreateData(EditorWorldDataImport editor_data)
	{
		EditorLoaderLog("EditorLoaderCreateData");
		EditorLoaderLog(string.Format("%1 created objects found", editor_data.EditorObjects.Count()));
		EditorLoaderLog(string.Format("%1 deleted objects found", editor_data.DeletedObjects.Count()));
		foreach (EditorObjectDataImport data_import: editor_data.EditorObjects) {
			EditorLoaderSpawnObject(data_import.Type, data_import.Position, data_import.Orientation);
		}

		foreach (int deleted_object: editor_data.DeletedObjects) {
			EditorLoaderDeleteObject(EditorLoaderModule.WorldObjects[deleted_object]);
		}
	}
	
	static void EditorLoaderSpawnObject(string type, vector position, vector orientation)
	{
		EditorLoaderLog(string.Format("Creating %1", type));
		if (GetGame().IsKindOf(type, "Man") || GetGame().IsKindOf(type, "DZ_LightAI")) {
			return;
		}
		
	    Object obj = GetGame().CreateObjectEx(type, position, ECE_SETUP | ECE_UPDATEPATHGRAPH | ECE_CREATEPHYSICS);
		
		if (!obj) {
			return;
		}
		
	    obj.SetPosition(position);
	    obj.SetOrientation(orientation);
	    obj.SetOrientation(obj.GetOrientation());
	    obj.SetFlags(EntityFlags.STATIC, false);
	    obj.Update();
		obj.SetAffectPathgraph(true, false);
		if (obj.CanAffectPathgraph()) { 
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(GetGame().UpdatePathgraphRegionByObject, 100, false, obj);
		}
	}

	static void EditorLoaderDeleteObject(Object obj)
	{
		EditorLoaderLog(string.Format("Deleting %1", obj));
		CF__ObjectManager.RemoveObject(obj);
	}
	
	override void OnClientReady(PlayerBase player, PlayerIdentity identity)
	{
		EditorLoaderLog("OnClientReady");
		foreach (EditorWorldDataImport data: m_WorldDataImports) {
			GetRPCManager().SendRPC("EditorLoaderModule", "EditorLoaderRemoteCreateData", new Param1<ref EditorWorldDataImport>(data), true, identity);
		}
	}
	
	override bool IsClient() 
	{
		return true;
	}
	
	override bool IsServer()
	{
		return true;
	}
	
	static void EditorLoaderLog(string msg)
	{
		PrintFormat("[EditorLoader] %1", msg);
	}
}
