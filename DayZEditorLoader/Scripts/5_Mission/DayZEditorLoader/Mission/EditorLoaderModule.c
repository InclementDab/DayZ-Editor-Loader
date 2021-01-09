typedef array<int> DeletedBuildingsPacket;

class EditorLoaderModule: JMModuleBase
{
	static bool ExportLootData = false;	
	
	protected ref EditorWorldCache m_EditorWorldCache;
	protected ref array<ref EditorWorldDataImport> m_WorldDataImports = {};
	protected ref array<int> m_WorldDeletedBuildings = {};
	
	void EditorLoaderModule()
	{
		GetRPCManager().AddRPC("EditorLoaderModule", "EditorLoaderRemoteDeleteBuilding", this);
	}
	
	void ~EditorLoaderModule()
	{
		delete m_WorldDataImports;
		delete m_WorldDeletedBuildings;
		delete m_EditorWorldCache;
	}

	bool CreateLoaderCache(string file_name)
	{
		// Adds all map objects to the WorldObjects array
		EditorWorldCache world_cache = new EditorWorldCache();
		array<Object> objects = {};
		array<CargoBase> cargos = {};
		GetGame().GetObjectsAtPosition(vector.Zero, 100000, objects, cargos);
		
		foreach (Object o: objects) {
			world_cache.Insert(o.GetID(), o.GetPosition());
		}
		
		FileSerializer serializer = new FileSerializer();
		if (!serializer.Open(file_name, FileMode.WRITE)) {
			Error("EditorLoader: Error Opening Cache");
			return false;
		}
		
		if (!serializer.Write(world_cache)) {
			Error("EditorLoader: Error Writing Cache");
			return false;
		}
		
		serializer.Close();
		
		return FileExist(file_name);
	}
	
	EditorWorldCache LoadLoaderCache(string file_name)
	{
		EditorWorldCache world_cache = new EditorWorldCache();

		FileSerializer serializer = new FileSerializer();
		if (!serializer.Open(file_name)) {
			Error("EditorLoader: Error Opening Cache");
			return null;
		}
		
		if (!serializer.Read(world_cache)) {
			Error("EditorLoader: Error Reading Cache");
			return null;
		}
		
		serializer.Close();
		
		return world_cache;
	}
	
	string GetCacheFileName()
	{
		string world_name;
		GetGame().GetWorldName(world_name);
		world_name.ToLower();
		
		return string.Format("$profile:/EditorFiles/%1.cache", world_name);
	}
	
	void EditorLoaderCreateBuilding(EditorObjectDataImport editor_object)
	{
		EditorLoaderLog(string.Format("Creating %1", editor_object.Type));
		
		// This will cause.... issues (Might remove in the future for Trader mod?)
		if (GetGame().IsKindOf(editor_object.Type, "Man") || GetGame().IsKindOf(editor_object.Type, "DZ_LightAI")) {
			return;
		}
		
	    Object obj = GetGame().CreateObjectEx(editor_object.Type, editor_object.Position, ECE_SETUP | ECE_UPDATEPATHGRAPH | ECE_CREATEPHYSICS);
		
		if (!obj) {
			return;
		}
		
		//obj.SetScale(editor_object.Scale);
	    obj.SetOrientation(editor_object.Orientation);
	    obj.SetFlags(EntityFlags.STATIC, false);
	    obj.Update();
		obj.SetAffectPathgraph(true, false);
		if (obj.CanAffectPathgraph()) { 
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(GetGame().UpdatePathgraphRegionByObject, 100, false, obj);
		}
	}

	// Worlds slowest method :)
	void EditorLoaderDeleteBuildings(array<int> id_list)
	{
		if (id_list.Count() == 0) {
			EditorLoaderLog("No deleted buildings found, skipping...");
			return;
		}
				
		map<int, Object> backup_world_objects;
		EditorLoaderLog(string.Format("Deleting %1 buildings", id_list.Count()));
		foreach (int id: id_list) {
			Object object = GetBuildingFromID(id);
			if (!object) {
				EditorLoaderLog("Reverting to old method of grabbing buildings. Hold on tight...");	
				
				if (!backup_world_objects) {
					backup_world_objects = new map<int, Object>();
					EditorLoaderLog("Loading World Objects into cache...");
		
					// Adds all map objects to the WorldObjects array
					array<Object> objects = {};
					array<CargoBase> cargos = {};
					GetGame().GetObjectsAtPosition(vector.Zero, 100000, objects, cargos);
			
					foreach (Object o: objects) {
						backup_world_objects.Insert(o.GetID(), o);
					}
					
					EditorLoaderLog(string.Format("Loaded %1 World Objects into emergency cache", backup_world_objects.Count()));
				}
				
				object = backup_world_objects[id];
			} 
				
			CF_ObjectManager.HideMapObject(object);
		}
		
		delete m_EditorWorldCache;
	}
	
	Object GetBuildingFromID(int id)
	{
		if (!m_EditorWorldCache) {
			m_EditorWorldCache = new EditorWorldCache();
			
			string cache_file = GetCacheFileName();
			if (!FileExist(cache_file)) {
				EditorLoaderLog("Cache file not found... creating (this may take a few minutes)");
				if (!CreateLoaderCache(cache_file)) {
					Error("Cache file creation failed");
					return null;
				}
			}
			
			m_EditorWorldCache = LoadLoaderCache(cache_file);
		}
		
		array<Object> objects = {};
		array<CargoBase> cargos = {};
		GetGame().GetObjectsAtPosition3D(m_EditorWorldCache[id], 1, objects, cargos);
		
		foreach (Object object: objects) {
			if (object.GetID() == id) {
				return object;
			}
		}
		
		return null;
	}
		
	void EditorLoaderRemoteDeleteBuilding(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
	{
		Param2<ref DeletedBuildingsPacket, bool> delete_params(new DeletedBuildingsPacket(), false);
		if (!ctx.Read(delete_params)) {
			return;
		}
		
		DeletedBuildingsPacket packet = delete_params.param1;		
		foreach (int id: packet) {
			m_WorldDeletedBuildings.Insert(id);
		}
		
		if (delete_params.param2) {
			EditorLoaderDeleteBuildings(m_WorldDeletedBuildings);
		}
	}	


	override void OnMissionStart()
	{
		EditorLoaderLog("OnMissionStart");
		
		// Everything below this line is the Server side syncronization :)
		if (IsMissionHost()) {
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
					m_WorldDeletedBuildings.Insert(deleted_object);
				}
				
				foreach (EditorObjectDataImport editor_object: editor_data.EditorObjects) {
					EditorLoaderCreateBuilding(editor_object);
				}
			}
			
			EditorLoaderDeleteBuildings(m_WorldDeletedBuildings);
			
			// Runs thread that watches for EditorLoaderModule.ExportLootData = true;
			thread ExportLootData();
		}
	}
	
	protected ref array<string> m_LoadedPlayers = {};
	override void OnInvokeConnect(PlayerBase player, PlayerIdentity identity)
	{		
		
		string id = String(identity.GetId());
		
		EditorLoaderLog("OnInvokeConnect");
		if (GetGame().IsServer() && (m_LoadedPlayers.Find(id) == -1)) {
			m_LoadedPlayers.Insert(id);
			thread SendClientData(identity);
		}
	}
		
	override void OnClientDisconnect(PlayerBase player, PlayerIdentity identity, string uid)
	{
		EditorLoaderLog("OnClientDisconnect");
		m_LoadedPlayers.Remove(m_LoadedPlayers.Find(uid));
	}
	
	
	private void SendClientData(PlayerIdentity identity)
	{
		DeletedBuildingsPacket deleted_packets();
		
		// Delete buildings on client side
		for (int i = 0; i < m_WorldDataImports.Count(); i++) {
			for (int j = 0; j < m_WorldDataImports[i].DeletedObjects.Count(); j++) {
				// Signals that its the final deletion in the final file
				bool finished = (i == m_WorldDataImports.Count() - 1 && j == m_WorldDataImports[i].DeletedObjects.Count() - 1);
				deleted_packets.Insert(m_WorldDataImports[i].DeletedObjects[j]);
				
				// Send in packages of 50
				if (deleted_packets.Count() >= 50) {
					GetRPCManager().SendRPC("EditorLoaderModule", "EditorLoaderRemoteDeleteBuilding", new Param2<ref DeletedBuildingsPacket, bool>(deleted_packets, false), true, identity);
					deleted_packets.Clear();
				}				
			}
		}
		
		
		// Find fullproof way to never send this if no buildings
		GetRPCManager().SendRPC("EditorLoaderModule", "EditorLoaderRemoteDeleteBuilding", new Param2<ref DeletedBuildingsPacket, bool>(deleted_packets, true), true, identity);
	}
	
	
	// Runs on both client AND server
	override void OnMissionFinish()
	{
		CF.ObjectManager.UnhideAllMapObjects(false);		
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
