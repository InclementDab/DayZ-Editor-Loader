class EditorWorldDataImport
{
	string MapName;
	vector CameraPosition;
	ref array<ref EditorObjectDataImport> EditorObjects = {};
	ref array<ref EditorDeletedObjectDataImport> EditorDeletedObjects = {};
}