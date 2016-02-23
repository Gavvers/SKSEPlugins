#pragma once

#include "skse/GameThreads.h"
#include "skse/GameTypes.h"

#include "skse/NiTypes.h"

#include <functional>

class NiSourceTexture;
class NiRenderedTexture;
class NiTriBasedGeom;
class TESNPC;
class BGSHeadPart;
class BSFaceGenNiNode;
class NiGeometry;
class NiTriStripsData;

class SKSETaskExportHead : public TaskDelegate
{
	virtual void Run();
	virtual void Dispose() { delete this; }

public:
	SKSETaskExportHead::SKSETaskExportHead(Actor * actor, BSFixedString nifPath, BSFixedString ddsPath);

	UInt32			m_formId;
	BSFixedString	m_nifPath;
	BSFixedString	m_ddsPath;
};

class SKSETaskRefreshTintMask : public TaskDelegate
{
	virtual void Run();
	virtual void Dispose() { delete this; };

public:
	SKSETaskRefreshTintMask::SKSETaskRefreshTintMask(Actor * actor, BSFixedString ddsPath);

	UInt32			m_formId;
	BSFixedString	m_ddsPath;
};

BGSTextureSet * GetTextureSetForPart(TESNPC * npc, BGSHeadPart * headPart);
std::pair<BGSTextureSet*, BGSHeadPart*> GetTextureSetForPartByName(TESNPC * npc, BSFixedString partName);
NiTriBasedGeom * GetHeadTriBasedGeom(Actor * actor, UInt32 partType);
void ExportTintMaskDDS(Actor * actor, BSFixedString filePath);
NiTriBasedGeom * GetTriBasedGeomByHeadPart(BSFaceGenNiNode * faceNode, BGSHeadPart * headPart);

void SaveSourceDDS(NiSourceTexture * pkSrcTexture, const char * pcFileName);
void SaveRenderedDDS(NiRenderedTexture * pkTexture, const char * pcFileName);

bool VisitObjects(NiAVObject * parent, std::function<bool(NiAVObject*)> functor);

NiTransform GetGeometryTransform(NiGeometry * geometry);

UInt16	GetStripLengthSum(NiTriStripsData * strips);
void GetTriangleIndices(NiTriStripsData * strips, UInt16 i, UInt16 v0, UInt16 v1, UInt16 v2);
