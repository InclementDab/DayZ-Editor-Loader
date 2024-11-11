// Legacy issues resolved
typedef MissionServer EditorLoaderModule;

modded class MissionServer
{
	static const string MAPGROUPPOS_STORAGE_EXPORT = "$storage:export\\mapgrouppos.xml";
	static const string MAPGROUPPOS_FILE = "$mission:\\mapgrouppos.xml";
	static const string ROOT_DIRECTORY = "$mission:\\EditorFiles";

	static bool ExportProxyData = false;

	protected ref array<ref EditorSaveData> m_WorldDataImports = { };

	void LoadCustomBuilds(inout array<string> custom_builds) { } // making this into a semi-colon deletes the array

	static vector GetObjectBoundingCenter(string object_typename)
	{
		Object object = GetGame().CreateStaticObjectUsingP3D(object_typename, vector.Zero, vector.Zero, 1);
		if (!object)
		{
			return vector.Zero;
		}
		vector offset = object.GetBoundingCenter();
		object.Delete();
		return offset;
	}

	EditorSaveData LoadBinFile(string file)
	{
		FileSerializer serializer = new FileSerializer();
		EditorSaveData save_data = new EditorSaveData();

		if (!serializer.Open(file, FileMode.READ))
		{
			Error("Failed to open file " + file);
			return null;
		}

		if (!save_data.Read(serializer))
		{
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

		if (!fh)
		{
			Error("Could not open file " + file);
			return null;
		}

		string line;
		while (FGets(fh, line) > 0)
		{
			jsonData = jsonData + "\n" + line;
		}

		bool success = serializer.ReadFromString(save_data, jsonData, error);
		if (error != string.Empty || !success)
		{
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

		array<string> dze_files = Directory.EnumerateFiles(ROOT_DIRECTORY, "*.dze", 5);

		// append all packed builds to this
		LoadCustomBuilds(dze_files);

		if (dze_files.Count() == 0)
		{
			return;
		}

		DateTime date = DateTime.Now();
		foreach (string file: dze_files) {
			EditorSaveData save_data;
			if (EditorSaveData.IsBinnedFile(file))
			{
				save_data = LoadBinFile(file);
			}
			else
			{
				save_data = LoadJsonFile(file);
			}

			if (!save_data)
			{
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
				if (!deleted_obj)
				{
					continue;
				}

				GetDayZGame().GetSuppressedObjectManager().Suppress(deleted_obj);
			}

			foreach (EditorObjectData editor_object: editor_data.EditorObjects) {
				// Do not spawn, it is Editor Only				
				if (editor_object.EditorOnly || editor_object.Type == string.Empty)
				{
					continue;
				}


				Object obj;
				string object_typename = editor_object.Type;

				if (File.WildcardMatch(object_typename, "*.p3d"))
				{
					obj = GetGame().CreateStaticObjectUsingP3D(object_typename, editor_object.Position - GetObjectBoundingCenter(object_typename), editor_object.Orientation, editor_object.Scale, false);
				}
				else
				{
					// ensure the object exists in a protected/public scope, or exists
					if (GetGame().ConfigGetInt(string.Format("CfgVehicles %1 scope", object_typename)) < 1)
					{
						PrintFormat("Object '%1' is scope 0", editor_object.Type);
						continue;
					}

					obj = GetGame().CreateObjectEx(object_typename, editor_object.Position, ECE_SETUP | ECE_CREATEPHYSICS | ECE_NOLIFETIME | ECE_DYNAMIC_PERSISTENCY);
				}

				if (!obj)
				{
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
				if (EntityAI.CastTo(ent, obj) && !editor_object.Simulate)
				{
					ent.DisableSimulation(!editor_object.Simulate);
				}

				// Update netlights to load the proper data
				SerializedBuilding networked_object = SerializedBuilding.Cast(obj);
				if (networked_object)
				{
					networked_object.Read(editor_object.Parameters);
				}
			}
		}

		// update pathgraph for all spawned objects
		GetGame().GetWorld().ProcessMarkedObjectsForPathgraphUpdate();

		float total_time = DateTime.Now() - date;
		PrintFormat("%1 objects created, %2 deleted (completed in %1s)", created_objects, deleted_objects, total_time / 1000.0);
	}

	override void AfterHiveInit()
	{
		super.AfterHiveInit();

		if (!ExportProxyData)
		{
			return;
		}

		if (!DeleteFile(MAPGROUPPOS_FILE))
		{
			Error(string.Format("Failed to delete %1", MAPGROUPPOS_FILE));
			return;
		}

		PrintFormat("Exporting Loot to %1", MAPGROUPPOS_STORAGE_EXPORT);
		DateTime date = DateTime.Now();
		GetCEApi().ExportProxyData();

		PrintFormat("Copying file %1 to %2...", MAPGROUPPOS_STORAGE_EXPORT, MAPGROUPPOS_FILE);
		if (!CopyFile(MAPGROUPPOS_STORAGE_EXPORT, MAPGROUPPOS_FILE))
		{
			Error(string.Format("Failed to copy %1 to %2", MAPGROUPPOS_FILE, MAPGROUPPOS_STORAGE_EXPORT));
			return;
		}

		if (!DeleteFile(MAPGROUPPOS_STORAGE_EXPORT))
		{
			Error(string.Format("Failed to delete %1", MAPGROUPPOS_STORAGE_EXPORT));
			return;
		}

		TimeSpan total_time = DateTime.Now() - date;
		PrintFormat("Export complete (took %1)", total_time.Format());
	}
}
