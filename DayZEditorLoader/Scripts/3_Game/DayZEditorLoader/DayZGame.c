typedef array<ref EditorDeletedObjectData> DeletedBuildingsPacket;

modded class DayZGame
{
	static const int RPC_REMOTE_DELETE_BUILDING = -34293538;

	// handle building deletions
	override void OnRPC(PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx)
	{
		super.OnRPC(sender, target, rpc_type, ctx);
		
		switch (rpc_type) {
			case RPC_REMOTE_DELETE_BUILDING: {
				if (GetGame().IsDedicatedServer()) {
					return;
				}
				
				DeletedBuildingsPacket deleted_buildings = new DeletedBuildingsPacket();
				if (!ctx.Read(deleted_buildings)) {
					return;
				}
				
				foreach (EditorDeletedObjectData deleted_building: deleted_buildings) {
					ObjectRemover.RemoveObject(deleted_building.FindObject());
				}
				break;
			}
		}
	}
}