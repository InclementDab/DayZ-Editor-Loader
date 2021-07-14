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

	override void OnMissionStart()
	{
		EditorLoaderLog("OnMissionStart");
		
		// Everything below this line is the Server side syncronization :)
		if (IsMissionHost()) {
			if (!MakeDirectory("$profile:EditorFiles")) {
				EditorLoaderLog("Could not create EditorFiles directory. Exiting...");
				return;
			}
	
			EditorSaveData data_import;				
			TStringArray files = FindFiles("*.dzebin");
			foreach (string bin_file: files) {
				EditorLoaderLog("File found: " + bin_file);
				FileSerializer serializer();
				if (!serializer) {
					Error("Failed to create FileSerializer");
					continue;
				}
				
				if (!serializer.Open("$profile:/EditorFiles/" + bin_file, FileMode.READ)) {
					Error("Failed to open file " + bin_file);
					continue;
				}
				
				data_import = new EditorSaveData();				
				if (!data_import.Read(serializer)) {
					Error("Failed to read file " + bin_file);
					serializer.Close();
					continue;
				}
								
				serializer.Close();
				m_WorldDataImports.Insert(data_import);
				EditorLoaderLog("Loaded $profile:/EditorFiles/" + bin_file);
			}
			
			files = FindFiles("*.dze");
			foreach (string file: files) {
				file.ToLower();
				if (file.Contains("dzebin")) {
					continue;
				}
				
				EditorLoaderLog("File found: " + file);
				JsonFileLoader<EditorSaveData>.JsonLoadFile("$profile:/EditorFiles/" + file, data_import);
				if (!data_import) {					
					Error("Data was invalid");
					continue;
				}
				
				data_import.MapName.ToLower();
				if (data_import.MapName != GetFormattedWorldName()) {
					Error("Wrong world loaded, current is " + GetFormattedWorldName() + ", file is made for " + data_import.MapName);
					//continue;
				}
				
				m_WorldDataImports.Insert(data_import);
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
