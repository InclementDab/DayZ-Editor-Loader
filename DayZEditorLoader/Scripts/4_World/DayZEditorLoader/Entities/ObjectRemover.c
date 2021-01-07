class ObjectRemoverBase: BuildingSuper
{
	protected ref array<Object> m_HiddenMapObjects;
	
	void ObjectRemoverBase()
	{
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(HideObjects, 100);
	}
	
	void ~ObjectRemoverBase()
	{
		CF.ObjectManager.UnhideMapObjects(m_HiddenMapObjects);
	}
	
	private void HideObjects()
	{
		m_HiddenMapObjects = CF.ObjectManager.HideMapObjectsInRadius(GetPosition(), GetRadius());
	}
	
	float GetRadius();
}

class ObjectRemover20x20: ObjectRemoverBase
{	
	override float GetRadius()
	{
		return 20;
	}
}