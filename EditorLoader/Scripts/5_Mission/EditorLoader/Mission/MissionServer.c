// Legacy issues resolved
typedef MissionServer EditorLoaderModule;

modded class MissionServer
{	
	static const string MAPGROUPPOS_STORAGE_EXPORT = "$storage:export\\mapgrouppos.xml";
	static const string MAPGROUPPOS_FILE = "$mission:\\mapgrouppos.xml";
	static const string ROOT_DIRECTORY = "$mission:\\EditorFiles";
	
	static bool ExportProxyData = false;
	
	protected ref array<ref EditorSaveData> m_WorldDataImports = {};
	
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

	void LoadDirectory(string directory, inout array<string> files)
	{
		string name;
		FileAttr attr;
		FindFileHandle handle = FindFile(string.Format("%1/*", directory), name, attr, FindFileFlags.ALL);

		if (name != string.Empty && name.Contains(".dze"))
			files.Insert(directory + "/" + name);

		while (FindNextFile(handle, name, attr))
		{
			if (name != string.Empty && name.Contains(".dze"))
				files.Insert(directory + "/" + name);
		}

		CloseFindFile(handle);
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
		super.OnMissionStart();
		MakeDirectory(ROOT_DIRECTORY);
				
		TStringArray files = {};
		#ifdef PLATFORM_LINUX
			LoadDirectory(ROOT_DIRECTORY, files);
		#else
			LoadFolder(ROOT_DIRECTORY, files);
		#endif

		
		// append all packed builds to this
		LoadCustomBuilds(files);
		if (files.Count() == 0) {
			return;
		}
		
		DateTime date = DateTime.Now();
		foreach (string file: files) {			
			EditorSaveData save_data;
			if (EditorSaveData.IsBinnedFile(file)) {
				save_data = LoadBinFile(file);
			} else {
				save_data = LoadJsonFile(file);
			}
			
			if (!save_data) {
				continue;
			}
			
			m_WorldDataImports.Insert(save_data);
		}
		
		int created_objects, deleted_objects;
		// Create and Delete buildings on Server Side
		foreach (EditorSaveData editor_data: m_WorldDataImports) {
			created_objects += editor_data.EditorObjects.Count();
			deleted_objects += editor_data.EditorHiddenObjects.Count();
			
			foreach (EditorDeletedObjectData deleted_object: editor_data.EditorHiddenObjects) {				
				Object deleted_obj = deleted_object.FindObject();
				if (!deleted_obj) {
					continue;
				}
				
				GetDayZGame().GetSuppressedObjectManager().Suppress(deleted_obj);
			}
			
			foreach (EditorObjectData editor_object: editor_data.EditorObjects) {	
				// Do not spawn, it is Editor Only				
				if (editor_object.EditorOnly || editor_object.Type == string.Empty) {
					continue;
				}

				// ensure the object exists in a protected/public scope, or exists
				if (GetGame().ConfigGetInt(string.Format("CfgVehicles %1 scope", editor_object.Type)) < 1) {
					PrintFormat("Object '%1' is scope 0", editor_object.Type);
					continue;
				}
				
				Object obj = GetGame().CreateObjectEx(editor_object.Type, editor_object.Position, ECE_SETUP | ECE_CREATEPHYSICS | ECE_NOLIFETIME | ECE_DYNAMIC_PERSISTENCY);
				if (!obj) {
					PrintFormat("Failed to create object %1", editor_object.Type);
					continue;
				}
								
				// disabled for letting 40mm in
				//obj.SetAllowDamage(editor_object.AllowDamage);
				obj.SetOrientation(editor_object.Orientation);
				obj.SetScale(editor_object.Scale);
				obj.Update();

				GetGame().GetWorld().MarkObjectForPathgraphUpdate(obj);
				
				// EntityAI cast stuff
				EntityAI ent;
				if (EntityAI.CastTo(ent, obj) && !editor_object.Simulate) {
					ent.DisableSimulation(!editor_object.Simulate);
				}
				
				// Update netlights to load the proper data
				SerializedBuilding networked_object = SerializedBuilding.Cast(obj);
				if (networked_object) {
					networked_object.Read(editor_object.Parameters);
				}
			}
		}

		// update pathgraph for all spawned objects
		GetGame().GetWorld().ProcessMarkedObjectsForPathgraphUpdate();
		
		TimeSpan total_time = DateTime.Now() - date;
		PrintFormat("%1 objects created, %2 deleted (completed in %1)", created_objects, deleted_objects, total_time.Format());
	}
	
	override void AfterHiveInit(Hive hive)
	{
		super.AfterHiveInit(hive);
		
		if (!hive || !ExportProxyData) {
			return;
		}
		
		if (!DeleteFile(MAPGROUPPOS_FILE)) {
			Error(string.Format("Failed to delete %1", MAPGROUPPOS_FILE));
			return;
		}
		
		PrintFormat("Exporting Loot to %1", MAPGROUPPOS_STORAGE_EXPORT);
		DateTime date = DateTime.Now();
		GetCEApi().ExportProxyData();
		
		PrintFormat("Copying file %1 to %2...", MAPGROUPPOS_STORAGE_EXPORT, MAPGROUPPOS_FILE);
		if (!CopyFile(MAPGROUPPOS_STORAGE_EXPORT, MAPGROUPPOS_FILE)) {
			Error(string.Format("Failed to copy %1 to %2", MAPGROUPPOS_FILE, MAPGROUPPOS_STORAGE_EXPORT));
			return;
		}
		
		if (!DeleteFile(MAPGROUPPOS_STORAGE_EXPORT)) {
			Error(string.Format("Failed to delete %1", MAPGROUPPOS_STORAGE_EXPORT));
			return;
		}
		
		TimeSpan total_time = DateTime.Now() - date;
		PrintFormat("Export complete (took %1)", total_time.Format());
	}
}
