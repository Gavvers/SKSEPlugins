#pragma once

#include "interfaces/IPluginInterface.h"

#include <unordered_map>
#include "skse/GameTypes.h"

#include "OverrideInterface.h"
#include "OverrideVariant.h"

class TESObjectREFR;
struct SKSESerializationInterface;
class NiNode;
class NiAVObject;
class NiTransform;

class NodeTransformKeys : public std::unordered_map<BSFixedString, OverrideRegistration<BSFixedString>>
{
public:
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion);
};

class NodeTransformRegistrationMapHolder : public SafeDataHolder<std::unordered_map<UInt64, MultiRegistration<MultiRegistration<NodeTransformKeys, 2>,2>>>
{
public:
	typedef std::unordered_map<UInt64, MultiRegistration<MultiRegistration<NodeTransformKeys, 2>,2>> RegMap;

	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, UInt64 * outHandle);
};

// Node names are hashed here due to some case where the node "NPC" gets overwritten for some unknown reason
class NodeTransformCache : public SafeDataHolder<std::unordered_map<BSFixedString, std::unordered_map<BSFixedString, NiTransform>>>
{
public:
	typedef std::unordered_map<BSFixedString, NiTransform> NodeMap;
	typedef std::unordered_map<BSFixedString, NodeMap> RegMap;

	NiTransform * GetBaseTransform(BSFixedString rootModel, BSFixedString nodeName, bool relative);
};

class NiTransformInterface : public IPluginInterface
{
public:
	enum
	{
		kCurrentPluginVersion = 2,
		kSerializationVersion1 = 1,
		kSerializationVersion2 = 2,
		kSerializationVersion = kSerializationVersion2
	};
	virtual UInt32 GetVersion();

	virtual void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	virtual bool Load(SKSESerializationInterface* intfc, UInt32 kVersion);
	virtual void Revert();

	virtual bool AddNodeTransform(TESObjectREFR * ref, bool firstPerson, bool isFemale, BSFixedString node, BSFixedString name, OverrideVariant & value);
	virtual bool RemoveNodeTransformComponent(TESObjectREFR * ref, bool firstPerson, bool isFemale, BSFixedString node, BSFixedString name, UInt16 key, UInt16 index);
	virtual bool RemoveNodeTransform(TESObjectREFR * refr, bool firstPerson, bool isFemale, BSFixedString node, BSFixedString name);
	virtual void RemoveAllReferenceTransforms(TESObjectREFR * refr);
	
	virtual OverrideVariant GetOverrideNodeValue(TESObjectREFR * refr, bool firstPerson, bool isFemale, BSFixedString node, BSFixedString name, UInt16 key, SInt8 index);
	virtual bool GetOverrideNodeTransform(TESObjectREFR * refr, bool firstPerson, bool isFemale, BSFixedString node, BSFixedString name, UInt16 key, NiTransform * result);

	virtual void GetOverrideTransform(OverrideSet * set, UInt16 key, NiTransform * result);
	virtual BSFixedString GetRootModelPath(TESObjectREFR * refr, bool firstPerson, bool isFemale);

	virtual void UpdateNodeAllTransforms(TESObjectREFR * ref);

	virtual void VisitNodes(TESObjectREFR * refr, bool firstPerson, bool isFemale, std::function<bool(BSFixedString, OverrideRegistration<BSFixedString>*)> functor);
	virtual bool VisitNodeTransforms(TESObjectREFR * refr, bool firstPerson, bool isFemale, BSFixedString node, std::function<bool(OverrideRegistration<BSFixedString>*)> each_key, std::function<void(NiNode*, NiAVObject*, NiTransform*)> finalize);
	virtual void UpdateNodeTransforms(TESObjectREFR * ref, bool firstPerson, bool isFemale, BSFixedString node);

	virtual void VisitStrings(std::function<void(BSFixedString)> functor);
	
	void RemoveInvalidTransforms(UInt64 handle);
	void RemoveNamedTransforms(UInt64 handle, BSFixedString name);
	void SetHandleNodeTransforms(UInt64 handle, bool immediate = false, bool reset = false);

	NodeTransformRegistrationMapHolder	transformData;
	NodeTransformCache					transformCache;
};