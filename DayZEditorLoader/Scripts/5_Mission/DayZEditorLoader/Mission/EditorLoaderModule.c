typedef array<ref EditorDeletedObjectData> DeletedBuildingsPacket;

class EditorLoaderModule: JMModuleBase
{
	static const string ROOT_DIRECTORY = "$mission:\\EditorFiles";
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
	
	void LoadFolder(string folder, inout array<string> files)
	{
		string folder_name, file_name;
		FileAttr file_attr;
		
		// scan for folders
		FindFileHandle folder_handle = FindFile(string.Format("%1\\*", folder), folder_name, file_attr, FindFileFlags.DIRECTORIES);
		if (folder_name != string.Empty && file_attr == FileAttr.DIRECTORY) {
			LoadFolder(folder + "\\" + folder_name + "\\", files);
		}
		
		while (FindNextFile(folder_handle, folder_name, file_attr)) {
			if (folder_name != string.Empty && file_attr == FileAttr.DIRECTORY) {
				LoadFolder(folder + "\\" + folder_name + "\\", files);
			}
		}
		
		CloseFindFile(folder_handle);
		
		// scan for dze files
		FindFileHandle file_handle = FindFile(string.Format("%1\\*.dze", folder), file_name, file_attr, FindFileFlags.ALL);
		if (file_name != string.Empty) {
			files.Insert(folder + "\\" + file_name);
		}
		
		while (FindNextFile(file_handle, file_name, file_attr)) {
			if (file_name != string.Empty) {
				files.Insert(folder + "\\" + file_name);
			}
		}
		
		CloseFindFile(file_handle);
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
		
		if (!MakeDirectory(ROOT_DIRECTORY)) {
			EditorLoaderLog("Could not create EditorFiles directory. Exiting...");
			return;
		}
		
		EditorSaveData data_import;
		
		TStringArray files = {};
		LoadFolder(ROOT_DIRECTORY, files);
		
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
			if (EditorSaveData.IsBinnedFile(file)) {
				save_data = LoadBinFile(file);
			} else {
				save_data = LoadJsonFile(file);
			}
			
			if (!save_data) {
				EditorLoaderLog("Failed to load " + file);
				continue;
			}
			
			m_WorldDataImports.Insert(save_data);
		}
		
		EditorLoaderLog("Loaded files in " + ((GetGame().GetTime() - time) / 1000) + "s");	
		int created_objects, deleted_objects;
		// Create and Delete buildings on Server Side
		foreach (EditorSaveData editor_data: m_WorldDataImports) {
			created_objects += editor_data.EditorObjects.Count();
			deleted_objects += editor_data.EditorDeletedObjects.Count();
			
			foreach (EditorDeletedObjectData deleted_object: editor_data.EditorDeletedObjects) {				
				Object deleted_obj = deleted_object.FindObject();
				if (!deleted_obj) {
					continue;
				}
				
				ObjectRemover.RemoveObject(deleted_obj);
			}
			
			foreach (EditorObjectData editor_object: editor_data.EditorObjects) {	
				// Do not spawn, it is Editor Only				
				if (editor_object.EditorOnly) {
					continue;
				}

				// ensure the object exists in a protected/public scope, or exists
				if (GetGame().ConfigGetInt("CfgVehicles " + editor_object.Type + " scope") < 1) {
					EditorLoaderLog("Object '" + editor_object.Type + "' is either private scope or does not exist");
					continue;
				}

				// prevent persistent objects from spawning, disable by setting 'disableEditorLoaderSafetyCheck = 1;' in serverDZ.cfg
				if (!GetGame().ServerConfigGetInt("disableEditorLoaderSafetyCheck") && !IsValidBuilding(editor_object.Type)) {
					EditorLoaderLog("Object '" + editor_object.Type + "' is a persistence object, skipping");
					continue;
				}
				
				Object obj = GetGame().CreateObjectEx(editor_object.Type, editor_object.Position, ECE_SETUP | ECE_UPDATEPATHGRAPH | ECE_CREATEPHYSICS);
				if (!obj) {
					continue;
				}
								
				obj.SetAllowDamage(editor_object.AllowDamage);
				obj.SetScale(editor_object.Scale);
				obj.SetOrientation(editor_object.Orientation);
				obj.SetFlags(EntityFlags.STATIC, false); // set object as static, will not persist (fail safe)
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
				}
			}
		}
		
		EditorLoaderLog(string.Format("%1 total objects created", created_objects));
		EditorLoaderLog(string.Format("%1 total objects deleted", deleted_objects));
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
	
	void EditorLoaderRemoteDeleteBuilding(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
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

	bool IsValidBuilding(string type)
	{
		// check if building is not persistent
		string destr = GetGame().ConfigGetTextOut("CfgVehicles " + type + " destrType");
		return (destr == "DestructNo" || destr == "DestructBuilding");
	}
}