typedef array<ref EditorDeletedObjectData> DeletedBuildingsPacket;

class EditorLoaderModule: JMModuleBase
{
	static const string RootDirectory = "$profile:/EditorFiles/";
	static bool ExportLootData = false;	
	
	protected ref array<ref EditorSaveData> m_WorldDataImports = {};
	
	void EditorLoaderModule()
	{
		GetRPCManager().AddRPC("EditorLoaderModule", "EditorLoaderRemoteDeleteBuilding", this);
	}
	
	void ~EditorLoaderModule()
	{
		delete m_WorldDataImports;
	}
	
	void LoadCustomBuilds(inout array<string> custom_builds) {} // making this into a semi-colon deletes the array

	TStringArray FindFiles(string extension = ".dze")
	{
		TStringArray files = {};
		string file_name;
		FileAttr file_attr;
		
		FindFileHandle find_handle = FindFile(string.Format("%1*%2", RootDirectory, extension), file_name, file_attr, FindFileFlags.ALL);
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
		if (!GetGame().IsServer() || !GetGame().IsMultiplayer()) {
			return;
		}
		
		if (!MakeDirectory(RootDirectory)) {
			EditorLoaderLog("Could not create EditorFiles directory. Exiting...");
			return;
		}

		EditorSaveData data_import;				
		TStringArray files = FindFiles("*.dze");
		
		// append all packed builds to this
		LoadCustomBuilds(files);
		
		if (files.Count() == 0) {
			EditorLoaderLog("No files found, exiting");
			return;
		}
		
		float time = GetGame().GetTime();
		foreach (string file: files) {
			EditorLoaderLog("File found: " + file);
			
			EditorSaveData save_data;
			if (EditorSaveData.IsBinnedFile(RootDirectory + file)) {
				save_data = LoadBinFile(RootDirectory + file);
			} else {
				save_data = LoadJsonFile(RootDirectory + file);
			}
			
			if (!save_data) {
				EditorLoaderLog("Failed to load " + file);
				continue;
			}
			
			m_WorldDataImports.Insert(save_data);
			EditorLoaderLog("Loaded " + RootDirectory + file);
		}
		
		EditorLoaderLog("Loaded files in " + ((GetGame().GetTime() - time) / 1000) + "s");	
		
		// Create and Delete buildings on Server Side
		foreach (EditorSaveData editor_data: m_WorldDataImports) {
			EditorLoaderLog(string.Format("%1 created objects found", editor_data.EditorObjects.Count()));
			EditorLoaderLog(string.Format("%1 deleted objects found", editor_data.EditorDeletedObjects.Count()));
			
			foreach (EditorDeletedObjectData deleted_object: editor_data.EditorDeletedObjects) {				
				Object deleted_obj = deleted_object.FindObject();
				if (!deleted_obj) {
					EditorLoaderLog("Skipping " + deleted_object.Type);
					continue;
				}
				
				ObjectRemover.RemoveObject(deleted_obj);
			}
			
			foreach (EditorObjectData editor_object: editor_data.EditorObjects) {	
				// Do not spawn, it is Editor Only				
				if (editor_object.EditorOnly) {
					continue;
				}
				
			    Object obj = GetGame().CreateObjectEx(editor_object.Type, editor_object.Position, ECE_SETUP | ECE_UPDATEPATHGRAPH | ECE_CREATEPHYSICS);
				if (!obj) {
					continue;
				}
								
				obj.SetAllowDamage(editor_object.AllowDamage);
				//obj.SetScale(editor_object.Scale);
			    obj.SetOrientation(editor_object.Orientation);
			    obj.Update();
				
				// EntityAI cast stuff
				EntityAI ent = EntityAI.Cast(obj);
				if (ent) {
					ent.DisableSimulation(!editor_object.Simulate);
				}
				
				// Update netlights to load the proper data
				NetworkLightBase netlight = NetworkLightBase.Cast(obj);
				if (netlight) {
					netlight.Read(editor_object.Parameters);
					netlight.SetSynchDirty();
				}
			}
		}
		
		EditorLoaderLog("Deleted & Created all objects in " + ((GetGame().GetTime() - time) / 1000) + "s");	
			
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
		float time = GetGame().GetTime();
		DeletedBuildingsPacket deleted_packets();
		
		// Delete buildings on client side
		for (int i = 0; i < m_WorldDataImports.Count(); i++) {
			for (int j = 0; j < m_WorldDataImports[i].EditorDeletedObjects.Count(); j++) {
				deleted_packets.Insert(m_WorldDataImports[i].EditorDeletedObjects[j]);
								
				// Send in packages of 100
				if (deleted_packets.Count() >= 100) {
					GetRPCManager().SendRPC("EditorLoaderModule", "EditorLoaderRemoteDeleteBuilding", new Param1<ref DeletedBuildingsPacket>(deleted_packets), true, identity);
					deleted_packets.Clear();
				}				
			}
		}
		
		if (deleted_packets.Count() > 0) {
			GetRPCManager().SendRPC("EditorLoaderModule", "EditorLoaderRemoteDeleteBuilding", new Param1<ref DeletedBuildingsPacket>(deleted_packets), true, identity);
		}
		
		EditorLoaderLog("Sent Deleted objects in " + ((GetGame().GetTime() - time) / 1000) + "s");	
	}
	
	void EditorLoaderRemoteDeleteBuilding(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
	{
		Param1<ref DeletedBuildingsPacket> delete_params(new DeletedBuildingsPacket());
		if (!ctx.Read(delete_params)) {
			return;
		}
		
		DeletedBuildingsPacket packet = delete_params.param1;		
		foreach (EditorDeletedObjectData deleted_building: packet) {
			ObjectRemover.RemoveObject(deleted_building.FindObject());
		}
	}	
	
	// Runs on both client AND server
	override void OnMissionFinish()
	{
		ObjectRemover.RestoreAllMapObjects();	
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