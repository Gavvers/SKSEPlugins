#pragma once

#include "interfaces/IPluginInterface.h"
#include "interfaces/IHashType.h"

#include "skse/GameTypes.h"
#include "skse/GameThreads.h"
#include "skse/NiTypes.h"

#include <string>
#include <set>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <ctime>

class TESObjectREFR;
struct SKSESerializationInterface;
class BSResourceNiBinaryStream;
class NiAVObject;
class TESObjectARMO;
class TESObjectARMA;
class NiExtraData;
class TESNPC;

#define MORPH_MOD_DIRECTORY "actors\\character\\BodyGenData\\"

class BodyMorph
{
public:
	bool operator<(const BodyMorph & rhs) const	{ return m_name < rhs.m_name; }
	bool operator==(const BodyMorph & rhs) const	{ return m_name == rhs.m_name; }

	BSFixedString	m_name;
	float			m_value;

	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion);
};

class BodyMorphSet : public std::set<BodyMorph>
{
public:
	// Serialization
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion);
};

class BodyMorphData : public std::unordered_map<BSFixedString, std::unordered_map<BSFixedString, float>>
{
public:
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion);
};

class ActorMorphs : public SafeDataHolder<std::unordered_map<UInt64, BodyMorphData>>
{
public:
	typedef std::unordered_map<UInt64, BodyMorphData>	MorphMap;

	// Serialization
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion);
};

class TriShapeVertexDelta
{
public:
	UInt16		index;
	NiPoint3	diff;
};

class TriShapePackedVertexDelta
{
public:
	UInt16	index;
	SInt16	x;
	SInt16	y;
	SInt16	z;
};

class TriShapeVertexData
{
public:
	virtual void ApplyMorph(UInt16 vertCount, NiPoint3 * vertices, float factor) = 0;
};
typedef std::shared_ptr<TriShapeVertexData> TriShapeVertexDataPtr;

class TriShapeFullVertexData : public TriShapeVertexData
{
public:
	virtual void ApplyMorph(UInt16 vertCount, NiPoint3 * vertices, float factor);

	std::vector<TriShapeVertexDelta> m_vertexDeltas;
};
typedef std::shared_ptr<TriShapeFullVertexData> TriShapeFullVertexDataPtr;

class TriShapePackedVertexData : public TriShapeVertexData
{
public:
	virtual void ApplyMorph(UInt16 vertCount, NiPoint3 * vertices, float factor);

	float									m_multiplier;
	std::vector<TriShapePackedVertexDelta>	m_vertexDeltas;
};
typedef std::shared_ptr<TriShapePackedVertexData> TriShapePackedVertexDataPtr;


class BodyMorphMap : public std::unordered_map<BSFixedString, TriShapeVertexDataPtr>
{
public:
	void ApplyMorphs(TESObjectREFR * refr, UInt16 vertexCount, NiPoint3* targetGeometry, NiPoint3 * storageGeometry) const;
	bool HasMorphs(TESObjectREFR * refr) const;
};

class TriShapeMap : public std::unordered_map<BSFixedString, BodyMorphMap>
{
public:
	TriShapeMap()
	{
		memoryUsage = sizeof(TriShapeMap);
		accessed = 0;
	}

	void ApplyMorphs(TESObjectREFR * refr, NiAVObject * rootNode, bool erase = false);
	void ApplyMorph(TESObjectREFR * refr, NiAVObject * rootNode, bool erase, const std::pair<BSFixedString, BodyMorphMap> & bodyMorph);

	UInt32 memoryUsage;
	std::time_t accessed;
};


class MorphCache : public SafeDataHolder<std::unordered_map<BSFixedString, TriShapeMap>>
{
	friend class BodyMorphInterface;

public:
	MorphCache()
	{
		totalMemory = sizeof(MorphCache);
		memoryLimit = totalMemory;
	}

	typedef std::unordered_map<BSFixedString, TriShapeMap>	FileMap;

	BSFixedString CreateTRIPath(const char * relativePath);
	bool CacheFile(const char * modelPath);

	void ApplyMorphs(TESObjectREFR * refr, NiAVObject * rootNode, bool erase = false);
	void UpdateMorphs(TESObjectREFR * refr);

	void Shrink();

private:
	UInt32 memoryLimit;
	UInt32 totalMemory;
};

class NIOVTaskUpdateModelWeight : public TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	NIOVTaskUpdateModelWeight(Actor * actor);
	
private:
	UInt32	m_formId;
};

class BodyGenMorphData
{
public:
	BSFixedString	name;
	float			lower;
	float			upper;
};

class BodyGenMorphSelector : public std::vector<BodyGenMorphData>
{
public:
	UInt32 Evaluate(std::function<void(BSFixedString, float)> eval);
};

class BodyGenMorphs : public std::vector<BodyGenMorphSelector>
{
public:
	UInt32 Evaluate(std::function<void(BSFixedString, float)> eval);
};

class BodyGenTemplate : public std::vector<BodyGenMorphs>
{
public:
	UInt32 Evaluate(std::function<void(BSFixedString, float)> eval);
};
typedef std::shared_ptr<BodyGenTemplate> BodyGenTemplatePtr;


typedef std::unordered_map<BSFixedString, BodyGenTemplatePtr> BodyGenTemplates;

class BodyTemplateList : public std::vector<BodyGenTemplatePtr>
{
public:
	UInt32 Evaluate(std::function<void(BSFixedString, float)> eval);
};

class BodyGenDataTemplates : public std::vector<BodyTemplateList>
{
public:
	UInt32 Evaluate(std::function<void(BSFixedString, float)> eval);
};
typedef std::shared_ptr<BodyGenDataTemplates> BodyGenDataTemplatesPtr;

typedef std::unordered_map<TESNPC*, BodyGenDataTemplatesPtr> BodyGenData;

class BodyMorphInterface : public IPluginInterface
{
public:
	enum
	{
		kCurrentPluginVersion = 3,
		kSerializationVersion1 = 1,
		kSerializationVersion2 = 2,
		kSerializationVersion = kSerializationVersion2
	};
	virtual UInt32 GetVersion();

	// Serialization
	virtual void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	virtual bool Load(SKSESerializationInterface * intfc, UInt32 kVersion);
	virtual void Revert();

	virtual void SetMorph(TESObjectREFR * actor, BSFixedString morphName, BSFixedString morphKey, float relative);
	virtual float GetMorph(TESObjectREFR * actor, BSFixedString morphName, BSFixedString morphKey);
	virtual void ClearMorph(TESObjectREFR * actor, BSFixedString morphName, BSFixedString morphKey);

	virtual float GetBodyMorphs(TESObjectREFR * actor, BSFixedString morphName);
	virtual void ClearBodyMorphNames(TESObjectREFR * actor, BSFixedString morphName);

	virtual void VisitMorphs(TESObjectREFR * actor, std::function<void(BSFixedString, std::unordered_map<BSFixedString, float> *)> functor);
	virtual void VisitKeys(TESObjectREFR * actor, BSFixedString name, std::function<void(BSFixedString, float)> functor);

	virtual void ClearMorphs(TESObjectREFR * actor);

	virtual void ApplyVertexDiff(TESObjectREFR * refr, NiAVObject * rootNode, bool erase = false);
	virtual void ApplyBodyMorphs(TESObjectREFR * refr);
	virtual void UpdateModelWeight(TESObjectREFR * refr, bool immediate = false);

	virtual void SetCacheLimit(UInt32 limit);
	virtual bool HasMorphs(TESObjectREFR * actor);

	virtual bool ReadBodyMorphs(BSFixedString filePath);
	virtual bool ReadBodyMorphTemplates(BSFixedString filePath);
	virtual UInt32 EvaluateBodyMorphs(TESObjectREFR * actor);

	virtual bool HasBodyMorph(TESObjectREFR * actor, BSFixedString morphName, BSFixedString morphKey);
	virtual bool HasBodyMorphName(TESObjectREFR * actor, BSFixedString morphName);
	virtual bool HasBodyMorphKey(TESObjectREFR * actor, BSFixedString morphKey);
	virtual void ClearBodyMorphKeys(TESObjectREFR * actor, BSFixedString morphKey);
	virtual void VisitStrings(std::function<void(BSFixedString)> functor);
	virtual void VisitActors(std::function<void(TESObjectREFR*)> functor);

private:
	ActorMorphs	actorMorphs;
	MorphCache	morphCache;
	BodyGenTemplates bodyGenTemplates;
	BodyGenData	bodyGenData;
};