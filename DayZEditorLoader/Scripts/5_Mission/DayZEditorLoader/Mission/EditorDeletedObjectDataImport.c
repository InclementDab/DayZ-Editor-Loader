class EditorDeletedObjectDataImport
{
	[NonSerialized()]
	int ID;
	
	string Type;
	vector Position;
	int Flags;
	
	[NonSerialized()]
	Object WorldObject;
			
	Object FindObject(float radius = 0.05)
	{
		array<Object> objects = {};
		array<CargoBase> cargos = {};
		GetGame().GetObjectsAtPosition3D(Position, radius, objects, cargos);
		
		foreach (Object object: objects) {
			if (object.GetType() == Type) {
				return object;
			}
		}
		
		return null;
	}
}