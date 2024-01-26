modded class MissionGameplay
{
	override void OnMissionFinish()
	{
		super.OnMissionFinish();
		
		ObjectRemover.RestoreAllMapObjects();	
	}
}