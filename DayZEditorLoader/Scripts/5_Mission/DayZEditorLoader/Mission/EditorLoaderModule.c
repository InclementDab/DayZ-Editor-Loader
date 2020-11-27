class EditorLoaderModule: JMModuleBase
{
	static bool ExportLootData = false;
	
	static ref map<int, ref OLinkT> WorldObjects;
	protected ref array<ref EditorWorldDataImport> m_WorldDataImports = {};
	
	static void LoadMapObjects()
	{
		WorldObjects = new map<int, ref OLinkT>();
		EditorLoaderLog("Loading World Objects into cache...");
				
		// Adds all map objects to the WorldObjects array
		ref array<Object> objects = {};
		ref array<CargoBase> cargos = {};
		GetGame().GetObjectsAtPosition(vector.Zero, 100000, objects, cargos);

		foreach (Object o: objects) {
			WorldObjects.Insert(o.GetID(), new OLinkT(o));
		}
		
				
		EditorLoaderLog(string.Format("Loaded %1 World Objects into cache", WorldObjects.Count()));
	}
	
	// Known bug: Buildings will stay deleted after joining a server
	// Find a way to undelete them

	// cant send vectors over RPC :ANGERY:
	void EditorLoaderCreateBuilding(string type, string position, string orientation)
	{
		EditorLoaderLog(string.Format("Creating %1", type));
		if (GetGame().IsKindOf(type, "Man") || GetGame().IsKindOf(type, "DZ_LightAI")) {
			return;
		}
		
	    Object obj = GetGame().CreateObjectEx(type, position.ToVector(), ECE_SETUP | ECE_UPDATEPATHGRAPH | ECE_CREATEPHYSICS);
		
		if (!obj) {
			return;
		}
		
	    obj.SetOrientation(orientation.ToVector());
	    obj.SetFlags(EntityFlags.STATIC, false);
	    obj.Update();
		obj.SetAffectPathgraph(true, false);
		if (obj.CanAffectPathgraph()) { 
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(GetGame().UpdatePathgraphRegionByObject, 100, false, obj);
		}
	}
	
	void EditorLoaderDeleteBuilding(int id)
	{
		if (!WorldObjects) {
			LoadMapObjects();
		}
		
		EditorLoaderLog(string.Format("Deleting %1", id));
		CF_ObjectManager.HideMapObject(WorldObjects[id].Ptr());
	}
	
	void EditorLoaderRemoteCreateBuilding(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
	{
		Param3<string, string, string> create_params;
		if (!ctx.Read(create_params)) {
			return;
		}
		
		EditorLoaderCreateBuilding(create_params.param1, create_params.param2, create_params.param3);
	}	
	
	void EditorLoaderRemoteDeleteBuilding(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
	{
		Param1<int> delete_params;
		if (!ctx.Read(delete_params)) {
			return;
		}
		
		EditorLoaderDeleteBuilding(delete_params.param1);
	}
	

	override void OnMissionStart()
	{
		EditorLoaderLog("OnMissionStart");
		
		GetRPCManager().AddRPC("EditorLoaderModule", "EditorLoaderRemoteCreateBuilding", this);
		GetRPCManager().AddRPC("EditorLoaderModule", "EditorLoaderRemoteDeleteBuilding", this);
		

		// Everything below this line is the Server side syncronization :)
		if (!IsMissionHost()) return;
	
		if (!FileExist("$profile:/EditorFiles")) {
			EditorLoaderLog("EditorFiles directory not found, creating...");
			if (!MakeDirectory("$profile:/EditorFiles")) {
				EditorLoaderLog("Could not create EditorFiles directory. Exiting...");
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
			EditorLoaderLog("File found: " + file);
			EditorWorldDataImport data_import;
			JsonFileLoader<EditorWorldDataImport>.JsonLoadFile("$profile:/EditorFiles/" + file, data_import);
			
			if (data_import) {
				m_WorldDataImports.Insert(data_import);
				EditorLoaderLog("Loaded $profile:/EditorFiles/" + file);
			}
		}
		
		
		// Create and Delete buildings on Server Side
		foreach (EditorWorldDataImport editor_data: m_WorldDataImports) {
			
			EditorLoaderLog(string.Format("%1 created objects found", editor_data.EditorObjects.Count()));
			EditorLoaderLog(string.Format("%1 deleted objects found", editor_data.DeletedObjects.Count()));
			
			foreach (int deleted_object: editor_data.DeletedObjects) {
				EditorLoaderDeleteBuilding(deleted_object);
			}
			
			foreach (EditorObjectDataImport editor_object: editor_data.EditorObjects) {
				EditorLoaderCreateBuilding(editor_object.Type, editor_object.Position.ToString(false), editor_object.Orientation.ToString(false));
			}
		}
				
		// Maybe having a massive map this big is hurting clients :)
		// Server side only
		if (WorldObjects) {
			WorldObjects.Clear();	
		}
		
		// Runs thread that watches for EditorLoaderModule.ExportLootData = true;
		thread ExportLootData();
	}

			
	override void OnClientReady(PlayerBase player, PlayerIdentity identity)
	{
		EditorLoaderLog("OnClientReady");
		
		thread SendClientData(player, identity);
	}
	
	private void SendClientData(PlayerBase player, PlayerIdentity identity)
	{
		// Create and Delete buildings on client side
		foreach (EditorWorldDataImport editor_data: m_WorldDataImports) {
				
			foreach (EditorObjectDataImport data_import: editor_data.EditorObjects) {
				GetRPCManager().SendRPC("EditorLoaderModule", "EditorLoaderRemoteCreateBuilding", new Param3<string, string, string>(data_import.Type, data_import.Position.ToString(false), data_import.Orientation.ToString(false)), true, identity, player);
			}
			
			foreach (int deleted_object: editor_data.DeletedObjects) {
				GetRPCManager().SendRPC("EditorLoaderModule", "EditorLoaderRemoteDeleteBuilding", new Param1<int>(deleted_object), true, identity, player);
			}			
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
	
		
	private void ExportLootData()
	{
		while (true) {
			if (GetCEApi() && ExportLootData) {
				GetCEApi().ExportProxyData(vector.Zero, 100000);
				return;
			}
			
			Sleep(1000);
		}
	}
}
