// Legacy issues resolved
typedef MissionServer EditorLoaderModule;

modded class MissionServer
{	
	static const string MAPGROUPPOS_STORAGE_EXPORT = "$storage:export\\mapgrouppos.xml";
	static const string MAPGROUPPOS_FILE = "$mission:\\mapgrouppos.xml";
	static const string ROOT_DIRECTORY = "$mission:\\EditorFiles";
	
	static bool ExportProxyData = false;
		
	void LoadCustomBuilds(inout array<string> custom_builds) {} // making this into a semi-colon deletes the array
		
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
			
		ScopedFunctionTimer function_timer("EditorLoader::MissionStart");
		
		MakeDirectory(ROOT_DIRECTORY);
		
		array<string> dze_files = Directory.EnumerateFiles(ROOT_DIRECTORY, "*.dze", 5);
		
		// append all packed builds to this
		LoadCustomBuilds(dze_files);
		
		if (dze_files.Count() == 0) {
			return;
		}
		
		function_timer.Dump(string.Format("Begin Loading %1 file(s)", dze_files.Count()));
		
		int created_objects, deleted_objects;
		// Create and Delete buildings on Server Side
		foreach (string file: dze_files) {	
			EditorSaveData save_data = null;
			if (EditorSaveData.IsBinnedFile(file)) {
				save_data = LoadBinFile(file);
			} else {
				save_data = LoadJsonFile(file);
			}
			
			if (!save_data) {
				continue;
			}
			
			created_objects += save_data.EditorObjects.Count();
			deleted_objects += save_data.EditorHiddenObjects.Count();
			
			array<Object> deleted_objects_list = {};
			foreach (EditorDeletedObjectData deleted_object: save_data.EditorHiddenObjects) {				
				Object deleted_obj = deleted_object.FindObject();
				if (deleted_obj) {
					deleted_objects_list.Insert(deleted_obj);
				}
			}
			
			GetDayZGame().GetSuppressedObjectManager().SuppressMany(deleted_objects_list, false);
			
			function_timer.Dump(string.Format("(%1) Deleted %2 Objects", file, deleted_objects_list.Count()));
			
			foreach (EditorObjectData editor_object: save_data.EditorObjects) {			
				// Do not spawn, it is Editor Only				
				if (editor_object.EditorOnly || editor_object.Type == string.Empty) {
					continue;
				}
				
				Object obj;
				string object_typename = editor_object.Type;
				if (File.WildcardMatch(object_typename, "*.p3d")) {
					object_typename = object_typename.Trim();
					object_typename.Replace("\/", "\\");
					obj = GetGame().CreateStaticObjectUsingP3D(object_typename, editor_object.Position, editor_object.Orientation, editor_object.Scale, false);
				} else {			
					obj = GetGame().CreateObjectEx(object_typename, editor_object.Position, ECE_SETUP | ECE_CREATEPHYSICS | ECE_NOLIFETIME | ECE_DYNAMIC_PERSISTENCY);
				}
				
				if (!obj) {
					PrintFormat("Object Create Failed %1", editor_object.Type);
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
			
			function_timer.Dump(string.Format("(%1) Created %2 Objects", file, save_data.EditorObjects.Count()));
		}
		
		// update pathgraph for all spawned objects
		GetGame().GetWorld().ProcessMarkedObjectsForPathgraphUpdate();
		
		function_timer.Dump(string.Format("Pathgraph Complete"));
	}
	
	override void AfterHiveInit()
	{
		super.AfterHiveInit();
		
		if (!ExportProxyData) {
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
