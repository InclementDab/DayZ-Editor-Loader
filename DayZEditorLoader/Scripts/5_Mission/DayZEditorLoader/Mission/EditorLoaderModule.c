typedef array<ref EditorDeletedObjectData> DeletedBuildingsPacket;

class EditorLoaderModule: JMModuleBase
{
	static bool ExportLootData = false;	
	
	protected ref array<ref EditorSaveData> m_WorldDataImports = {};
	protected ref array<ref EditorObjectData> m_WorldCreatedBuildings = {};
	protected ref array<ref EditorDeletedObjectData> m_WorldDeletedBuildings = {};
	
	void EditorLoaderModule()
	{
		GetRPCManager().AddRPC("EditorLoaderModule", "EditorLoaderRemoteDeleteBuilding", this);
	}
	
	void ~EditorLoaderModule()
	{
		delete m_WorldDataImports;
		delete m_WorldCreatedBuildings;
		delete m_WorldDeletedBuildings;
	}
		
	void EditorLoaderCreateBuildings(array<ref EditorObjectData> editor_objects)
	{
		EditorLoaderLog(string.Format("Creating %1 buildings", editor_objects.Count()));
		foreach (EditorObjectData editor_object: editor_objects) {
			
			// This will cause.... issues (Might remove in the future for Trader mod?)
			if (GetGame().IsKindOf(editor_object.Type, "Man") || GetGame().IsKindOf(editor_object.Type, "DZ_LightAI")) {
				continue;
			}
			
		    Object obj = GetGame().CreateObjectEx(editor_object.Type, editor_object.Position, ECE_SETUP | ECE_UPDATEPATHGRAPH | ECE_CREATEPHYSICS);

			if (!obj) {
				continue;
			}
			
			//obj.SetScale(editor_object.Scale);
		    obj.SetOrientation(editor_object.Orientation);
		    obj.SetFlags(EntityFlags.STATIC, false);
		    obj.Update();
		}
	}

	void EditorLoaderDeleteBuildings(array<ref EditorDeletedObjectData> building_list)
	{
		if (building_list.Count() == 0) {
			EditorLoaderLog("No deleted buildings found, skipping...");
			return;
		}
		
		EditorLoaderLog(string.Format("Deleting %1 buildings", building_list.Count()));
		foreach (EditorDeletedObjectData deleted_building: building_list) {	
			CF_ObjectManager.HideMapObject(deleted_building.FindObject());
		}
	}
			
	void EditorLoaderRemoteDeleteBuilding(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
	{
		Param2<ref DeletedBuildingsPacket, bool> delete_params(new DeletedBuildingsPacket(), false);
		if (!ctx.Read(delete_params)) {
			return;
		}
		
		DeletedBuildingsPacket packet = delete_params.param1;		
		foreach (EditorDeletedObjectData deleted_building: packet) {
			m_WorldDeletedBuildings.Insert(deleted_building);
		}
		
		if (delete_params.param2) {
			EditorLoaderDeleteBuildings(m_WorldDeletedBuildings);
		}
	}	

	TStringArray FindFiles(string extension = ".dze")
	{
		TStringArray files = {};
		string file_name;
		FileAttr file_attr;
		
		FindFileHandle find_handle = FindFile(string.Format("$profile:/EditorFiles/*%1", extension), file_name, file_attr, FindFileFlags.ALL);
		files.Insert(file_name);
		
		while (FindNextFile(find_handle, file_name, file_attr)) {
			files.Insert(file_name);
		}
		
		CloseFindFile(find_handle);
		return files;
	}
	
	EditorSaveData LoadBinFile(string file)
	{				
		FileSerializer serializer = new FileSerializer();
		EditorSaveData save_data = new EditorSaveData();		
		
		if (!serializer.Open(file, FileMode.READ)) {
			Error("Failed to open file " + file);
			return null;
		}
				
		if (!save_data.Read(serializer)) {
			Error("Failed to read file " + file);
			serializer.Close();
			return null;
		}
		
		serializer.Close();		
		return save_data;
	}
	
	EditorSaveData LoadJsonFile(string file)
	{
		JsonSerializer serializer = new JsonSerializer();
		EditorSaveData save_data = new EditorSaveData();
		FileHandle fh = OpenFile(file, FileMode.READ);
		string jsonData;
		string error;

		if (!fh) {
			Error("Could not open file " + file);
			return null;
		}
		
		string line;
		while (FGets(fh, line) > 0) {
			jsonData = jsonData + "\n" + line;
		}

		bool success = serializer.ReadFromString(save_data, jsonData, error);
		if (error != string.Empty || !success) {
			Error(error);
			return null;
		}
		
		CloseFile(fh);
		return save_data;
	}

	override void OnMissionStart()
	{
		EditorLoaderLog("OnMissionStart");
		
		// Everything below this line is the Server side syncronization :)
		if (!IsMissionHost()) {
			return;
		}
		
		if (!MakeDirectory("$profile:EditorFiles")) {
			EditorLoaderLog("Could not create EditorFiles directory. Exiting...");
			return;
		}

		EditorSaveData data_import;				
		TStringArray files = FindFiles("*.dze");
		Serializer serializer;
		
		foreach (string file: files) {
			EditorLoaderLog("File found: " + file);
			
			EditorSaveData save_data;
			if (file.Contains("dzebin")) {
				save_data = LoadBinFile("$profile:/EditorFiles/" + file);
			} else {
				save_data = LoadJsonFile("$profile:/EditorFiles/" + file);
			}
			
			if (!save_data) {
				EditorLoaderLog("Failed to load " + file);
				continue;
			}
			
			m_WorldDataImports.Insert(save_data);
			EditorLoaderLog("Loaded $profile:/EditorFiles/" + file);
		}
		
		// Create and Delete buildings on Server Side
		foreach (EditorSaveData editor_data: m_WorldDataImports) {
			EditorLoaderLog(string.Format("%1 created objects found", editor_data.EditorObjects.Count()));
			EditorLoaderLog(string.Format("%1 deleted objects found", editor_data.EditorDeletedObjects.Count()));
			
			foreach (EditorDeletedObjectData deleted_object: editor_data.EditorDeletedObjects) {
				m_WorldDeletedBuildings.Insert(deleted_object);
			}
			
			foreach (EditorObjectData editor_object: editor_data.EditorObjects) {
				m_WorldCreatedBuildings.Insert(editor_object);
			}
		}
		
		EditorLoaderDeleteBuildings(m_WorldDeletedBuildings);
		EditorLoaderCreateBuildings(m_WorldCreatedBuildings);
		
		// Runs thread that watches for EditorLoaderModule.ExportLootData = true;
		thread ExportLootData();
	}
	
	string GetFormattedWorldName()
	{
		string world_name;
		GetGame().GetWorldName(world_name);
		world_name.ToLower();
		return world_name;
	}
	
	protected ref array<string> m_LoadedPlayers = {};
	override void OnInvokeConnect(PlayerBase player, PlayerIdentity identity)
	{		
		string id = String(identity.GetId());
		
		EditorLoaderLog("OnInvokeConnect");
		if (GetGame().IsServer() && (m_LoadedPlayers.Find(id) == -1)) {
			m_LoadedPlayers.Insert(id);
			SendClientData(identity);
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
			for (int j = 0; j < m_WorldDataImports[i].EditorDeletedObjects.Count(); j++) {
				// Signals that its the final deletion in the final file
				bool finished = (i == m_WorldDataImports.Count() - 1 && j == m_WorldDataImports[i].EditorDeletedObjects.Count() - 1);
				deleted_packets.Insert(m_WorldDataImports[i].EditorDeletedObjects[j]);
				
				// Send in packages of 100
				if (deleted_packets.Count() >= 100) {
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
