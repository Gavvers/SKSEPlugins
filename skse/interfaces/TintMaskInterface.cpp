#include "TintMaskInterface.h"
#include "ShaderUtilities.h"

#include "skse/GameData.h"
#include "skse/GameReferences.h"
#include "skse/GameObjects.h"
#include "skse/GameRTTI.h"

#include "skse/GameStreams.h"

#include "skse/NiGeometry.h"
#include "skse/NiExtraData.h"
#include "skse/NiRTTI.h"
#include "skse/NiProperties.h"
#include "skse/NiMaterial.h"
#include "skse/NiRenderer.h"

#include <Shlwapi.h>

#include "tinyxml2.h"

#include <vector>
#include <algorithm>

extern TintMaskInterface	g_tintMaskInterface;

UInt32 TintMaskInterface::GetVersion()
{
	return kCurrentPluginVersion;
}

void TintMaskInterface::CreateTintsFromData(tArray<TintMask*> & masks, UInt32 size, const char ** textureData, SInt32 * colorData, float * alphaData, ColorMap & overrides)
{
	masks.Allocate(size);
	for(UInt32 m = 0; m < masks.count; m++) {
		//0107D060
		TintMask * tintMask = new TintMask;
		void* memory = FormHeap_Allocate(sizeof(TESTexture));
		memset(memory, 0, sizeof(TESTexture));
		((UInt32*)memory)[0] = 0x0107D060;
		TESTexture* newTexture = (TESTexture*)memory;
		newTexture->Init();
		newTexture->str = textureData[m];
		tintMask->texture = newTexture;

		UInt32	color = colorData[m];
		float	alpha = alphaData[m];

		auto & it = overrides.find(m);
		if (it != overrides.end()) {
			color = it->second & 0xFFFFFF;
			alpha = (it->second >> 24) / 255.0;
		}

		tintMask->color.red = (color >> 16) & 0xFF;
		tintMask->color.green = (color >> 8) & 0xFF;
		tintMask->color.blue = color & 0xFF;
		tintMask->color.alpha = 0;
		tintMask->tintType = TintMask::kMaskType_SkinTone;
		tintMask->alpha = alpha;

		masks.arr.entries[m] = tintMask;
	}
}

void TintMaskInterface::ReleaseTintsFromData(tArray<TintMask*> & masks)
{
	// Cleanup tint array
	for(UInt32 m = 0; m < masks.count; m++) {
		TintMask * tintMask = NULL;
		if (masks.GetNthItem(m, tintMask)) {
			TESTexture* texture = tintMask->texture;
			if (texture)
				FormHeap_Free(texture);
			delete tintMask;
		}
	}
	FormHeap_Free(masks.arr.entries);
}

BSRenderTargetGroup * CreateMaskTarget(UInt32 width, UInt32 height)
{
	BSRenderTargetGroup * target = NULL;
	if (width == 0 || height == 0)
		target = CALL_MEMBER_FN(NiRenderManager::GetSingleton(), CreateRenderTarget)(NiDX9Renderer::GetSingleton(), 32, 0, 0);
	else {
		BSFixedString mask("ArmorMask");
		NiTexture::FormatPrefs format;
		format.pixelLayout = NiTexture::FormatPrefs::kPixelLayoutTrueColor32;
		format.alphaFormat = NiTexture::FormatPrefs::kAlphaFormatSmooth;
		format.mipMapped = NiTexture::FormatPrefs::kMipFlagDefault;
		target = CreateRenderTargetGroup(&mask, width, height, &format, 0, 1, 0, 0, 0, 0, 0);
	}

	return target;
}

NIOVTaskDeferredMask::NIOVTaskDeferredMask(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, std::function<void(ColorMap*)> overrides)
{
	m_firstPerson = isFirstPerson;
	m_formId = refr->formID;
	m_armorId = armor->formID;
	m_addonId = addon->formID;
	m_object = object;
	m_overrides = overrides;
	m_object->IncRef();
}

void NIOVTaskDeferredMask::Dispose()
{
	m_object->DecRef();
}

void NIOVTaskDeferredMask::Run()
{
	TESForm * refrForm = LookupFormByID(m_formId);
	TESForm * armorForm = LookupFormByID(m_armorId);
	TESForm * addonForm = LookupFormByID(m_addonId);
	if (refrForm && armorForm && addonForm) {
		TESObjectREFR * refr = DYNAMIC_CAST(refrForm, TESForm, TESObjectREFR);
		TESObjectARMO * armor = DYNAMIC_CAST(armorForm, TESForm, TESObjectARMO);
		TESObjectARMA * addon = DYNAMIC_CAST(addonForm, TESForm, TESObjectARMA);

		if (refr && armor && addon) {
			g_tintMaskInterface.ApplyMasks(refr, m_firstPerson, armor, addon, m_object, m_overrides);
		}
	}
}

void TintMaskInterface::ApplyMasks(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * rootNode, std::function<void(ColorMap*)> overrides)
{
	MaskList maskList;
	VisitObjects(rootNode, [&](NiAVObject* object)
	{
		ObjectMask mask;
		if (mask.object = object->GetAsNiGeometry()) {
			auto textureData = ni_cast(object->GetExtraData(BSFixedString("MASKT").data), NiStringsExtraData);
			if (textureData) {
				mask.layerCount = textureData->m_size;
				mask.textureData = (const char**)textureData->m_data;
			}
			auto colorData = ni_cast(object->GetExtraData(BSFixedString("MASKC").data), NiIntegersExtraData);
			if (colorData) {
				mask.colorData = colorData->m_data;
				if (mask.layerCount != colorData->m_size)
					mask.layerCount = 0;
			}

			auto alphaData = ni_cast(object->GetExtraData(BSFixedString("MASKA").data), NiFloatsExtraData);
			if (alphaData) {
				mask.alphaData = alphaData->m_data;
				if (mask.layerCount != alphaData->m_size)
					mask.layerCount = 0;
			}

			auto resolutionWData = ni_cast(object->GetExtraData(BSFixedString("MASKR").data), NiIntegerExtraData);
			if (resolutionWData) {
				mask.resolutionWData = resolutionWData->m_data;
				mask.resolutionHData = resolutionWData->m_data;
			}
			else {
				auto resolutionWData = ni_cast(object->GetExtraData(BSFixedString("MASKW").data), NiIntegerExtraData);
				if (resolutionWData)
					mask.resolutionWData = resolutionWData->m_data;

				auto resolutionHData = ni_cast(object->GetExtraData(BSFixedString("MASKH").data), NiIntegerExtraData);
				if (resolutionHData)
					mask.resolutionHData = resolutionHData->m_data;
			}
			

			if (mask.object && mask.layerCount > 0)
				maskList.push_back(mask);
		}

		return false;
	});

	m_modelMap.ApplyLayers(refr, isFirstPerson, addon, rootNode, [&](NiGeometry* geom, MaskLayerTuple* mask)
	{
		ObjectMask obj;
		obj.object = geom;
		obj.resolutionWData = std::get<0>(*mask);
		obj.resolutionHData = std::get<1>(*mask);
		obj.layerCount = std::get<2>(*mask).size();
		obj.textureData = &std::get<2>(*mask)[0];
		obj.colorData = &std::get<3>(*mask)[0];
		obj.alphaData = &std::get<4>(*mask)[0];
		maskList.push_back(obj);
	});

	ColorMap overrideMap;
	if (overrides && !maskList.empty()) {
		overrides(&overrideMap);
	}

	for (auto & mask : maskList)
	{
		if(mask.object && mask.textureData && mask.colorData && mask.alphaData)
		{
			BSShaderProperty * shaderProperty = niptr_cast<BSShaderProperty>(mask.object->m_spEffectState);
			if(shaderProperty) {
				shaderProperty->IncRef();
				BSLightingShaderProperty * lightingShader = ni_cast(shaderProperty, BSLightingShaderProperty);
				if(lightingShader) {
					BSLightingShaderMaterial * material = (BSLightingShaderMaterial *)lightingShader->material;
					
					BSRenderTargetGroup * newTarget = NULL;
					if (m_maskMap.IsCaching())
						newTarget = m_maskMap.GetRenderTargetGroup(lightingShader);
					if (!newTarget) {
						UInt32 width = 0;
						UInt32 height = 0;
						if (mask.resolutionWData)
							width = mask.resolutionWData;
						if (mask.resolutionHData)
							height = mask.resolutionHData;
						else
							height = mask.resolutionWData;

						newTarget = CreateMaskTarget(width, height);
						if (newTarget && m_maskMap.IsCaching()) {
							m_maskMap.AddRenderTargetGroup(lightingShader, newTarget);
						}
					}
					if(newTarget) {
						tArray<TintMask*> tintMasks;
						CreateTintsFromData(tintMasks, mask.layerCount, mask.textureData, mask.colorData, mask.alphaData, overrideMap);

						struct Target
						{
							BSRenderTargetGroup * newTarget;
							UInt32	unk04;
							UInt32	unk08;
							UInt32	unk0C;
							UInt32	unk10;
						};

						Target target;
						target.newTarget = newTarget;
						target.unk04 = 0;
						target.unk08 = 0;
						target.unk0C = 0;
						target.unk10 = 0;

						NiRenderedTexture * renderedTexture = newTarget->renderedTexture[0];

						newTarget->IncRef();
						if (ApplyMasksToRenderTarget(&tintMasks, (BSRenderTargetGroup **)&target)) {
							BSMaskedShaderMaterial * tintedMaterial = static_cast<BSMaskedShaderMaterial*>(CreateShaderMaterial(BSMaskedShaderMaterial::kShaderType_FaceGen));
							CALL_MEMBER_FN(tintedMaterial, CopyFrom)(material);
							tintedMaterial->renderedTexture = renderedTexture;
							CALL_MEMBER_FN(lightingShader, SetFlags)(0x0A, true); // Enable detailmap
							CALL_MEMBER_FN(lightingShader, SetFlags)(0x15, false); // Disable FaceGen_RGB
							//material->ReleaseTextures();
							CALL_MEMBER_FN(lightingShader, SetMaterial)((BSMaskedShaderMaterial*)tintedMaterial, 1); // New material takes texture ownership
							if (renderedTexture) // Let the material now take ownership since the old target is destroyed now
								renderedTexture->DecRef();
							CALL_MEMBER_FN(lightingShader, InitializeShader)(mask.object);
						}

						newTarget->DecRef();

						ReleaseTintsFromData(tintMasks);
					}
				}
				shaderProperty->DecRef();
			}
		}
	}
}

void TintMaskMap::ManageRenderTargetGroups()
{
	SimpleLocker<TintMaskCacheMap> locker(this);
	m_caching = true;
}

BSRenderTargetGroup * TintMaskMap::GetRenderTargetGroup(BSLightingShaderProperty* key)
{
	auto & it = m_data.find(key);
	if (it != m_data.end())
		return it->second;

	return NULL;
}

void TintMaskMap::AddRenderTargetGroup(BSLightingShaderProperty* key, BSRenderTargetGroup* value)
{
	SimpleLocker<TintMaskCacheMap> locker(this);
	auto & it = m_data.emplace(key, value);
	if (it.second == true) {
		key->IncRef();
		value->IncRef();
	}
}

void TintMaskMap::ReleaseRenderTargetGroups()
{
	SimpleLocker<TintMaskCacheMap> locker(this);
	for (auto & it : m_data) {
		it.first->DecRef();
		it.second->DecRef();
	}
	m_data.clear();
	m_caching = false;
}

void BSReadAll(BSResourceNiBinaryStream* fin, std::string * out)
{
	char ch;
	UInt32 ret = fin->Read(&ch, 1);
	while (ret > 0) {
		out->push_back(ch);
		ret = fin->Read(&ch, 1);
	}
}

MaskLayerTuple * MaskDiffuseMap::GetMaskLayers(BSFixedString texture)
{
	auto & it = find(texture.data);
	if (it != end()) {
		return &it->second;
	}

	return NULL;
}

MaskDiffuseMap * MaskTriShapeMap::GetDiffuseMap(BSFixedString triShape)
{
	auto & it = find(triShape.data);
	if (it != end()) {
		return &it->second;
	}

	return NULL;
}

MaskLayerTuple * MaskModelMap::GetMask(BSFixedString nif, BSFixedString trishape, BSFixedString diffuse)
{
	return &m_data[nif.data][trishape.data][diffuse.data];
}

MaskTriShapeMap * MaskModelMap::GetTriShapeMap(BSFixedString nifPath)
{
	auto & it = m_data.find(nifPath.data);
	if (it != m_data.end()) {
		return &it->second;
	}

	return NULL;
}

bool ApplyMaskData(MaskTriShapeMap * triShapeMap, NiAVObject * object, const char * nameOverride, std::function<void(NiGeometry*, MaskLayerTuple*)> functor)
{
	if (NiGeometry * geometry = object->GetAsNiGeometry()) {
		geometry->IncRef();
		auto textureMap = triShapeMap->GetDiffuseMap(nameOverride ? BSFixedString(nameOverride) : object->m_name);
		if (textureMap) {
			NiProperty * shaderProperty = niptr_cast<NiProperty>(geometry->m_spEffectState);
			if (shaderProperty) {
				shaderProperty->IncRef();
				BSLightingShaderProperty * lightingShader = ni_cast(shaderProperty, BSLightingShaderProperty);
				if (lightingShader) {
					BSLightingShaderMaterial * material = (BSLightingShaderMaterial *)lightingShader->material;
					if (material->textureSet) {
						auto layerList = textureMap->GetMaskLayers(material->textureSet->GetTexturePath(0));
						if (layerList) {
							functor(geometry, layerList);
						}
					}
				}
				shaderProperty->DecRef();
			}
			
			return true;
		}
		geometry->DecRef();
	}

	return false;
}

void MaskModelMap::ApplyLayers(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMA * arma, NiAVObject * node, std::function<void(NiGeometry*, MaskLayerTuple*)> functor)
{
	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if (actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	SimpleLocker<MaskModelContainer> locker(this);

	auto triShapeMap = GetTriShapeMap(arma->models[isFirstPerson == true ? 1 : 0][gender].GetModelName());
	if (!triShapeMap) {
		triShapeMap = GetTriShapeMap(arma->models[isFirstPerson == true ? 1 : 0][gender].GetModelName());
		if (!triShapeMap) {
			return;
		}
	}

	UInt32 count = 0;
	VisitObjects(node, [&](NiAVObject* object)
	{
		if (ApplyMaskData(triShapeMap, object, NULL, functor))
			count++;

		return false;
	});

	if (count == 0)
		ApplyMaskData(triShapeMap, node, "", functor);
}

void TintMaskInterface::ReadTintData(LPCTSTR lpFolder, LPCTSTR lpFilePattern)
{
	TCHAR szFullPattern[MAX_PATH];
	WIN32_FIND_DATA FindFileData;
	HANDLE hFindFile;
	// first we are going to process any subdirectories
	PathCombine(szFullPattern, lpFolder, "*");
	hFindFile = FindFirstFile(szFullPattern, &FindFileData);
	if (hFindFile != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				// found a subdirectory; recurse into it
				PathCombine(szFullPattern, lpFolder, FindFileData.cFileName);
				if (FindFileData.cFileName[0] == '.')
					continue;
				ReadTintData(szFullPattern, lpFilePattern);
			}
		} while (FindNextFile(hFindFile, &FindFileData));
		FindClose(hFindFile);
	}
	// now we are going to look for the matching files
	PathCombine(szFullPattern, lpFolder, lpFilePattern);
	hFindFile = FindFirstFile(szFullPattern, &FindFileData);
	if (hFindFile != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				// found a file; do something with it
				PathCombine(szFullPattern, lpFolder, FindFileData.cFileName);
				ParseTintData(szFullPattern);
			}
		} while (FindNextFile(hFindFile, &FindFileData));
		FindClose(hFindFile);
	}
}

void TintMaskInterface::ParseTintData(LPCTSTR filePath)
{
	std::string path(filePath);
	path.erase(0, 5);

	BSResourceNiBinaryStream bStream(path.c_str());
	std::string data;
	BSReadAll(&bStream, &data);

	tinyxml2::XMLDocument tintDoc;
	tintDoc.Parse(data.c_str(), data.size());

	if (tintDoc.Error()) {
		_ERROR("%s", tintDoc.GetErrorStr1());
		return;
	}

	auto element = tintDoc.FirstChildElement("tintmasks");
	if (element) {
		auto object = element->FirstChildElement("object");
		while (object)
		{
			auto objectPath = BSFixedString(object->Attribute("path"));
			auto child = object->FirstChildElement("geometry");
			while (child)
			{
				auto trishapeName = child->Attribute("name");
				auto trishape = BSFixedString(trishapeName ? trishapeName : "");
				auto diffuse = BSFixedString(child->Attribute("diffuse"));
				auto width = child->IntAttribute("width");
				auto height = child->IntAttribute("height");
				auto priority = child->IntAttribute("priority");

				if (width && !height)
					height = width;
				if (height && !width)
					width = height;

				auto tuple = m_modelMap.GetMask(objectPath, trishape, diffuse);

				// If the current entry is of higher priorty, skip the new entry
				auto previousPriority = std::get<5>(*tuple);
				if (previousPriority > 0 && priority < previousPriority) {
					child = child->NextSiblingElement("geometry");
					continue;
				}

				std::get<0>(*tuple) = width;
				std::get<1>(*tuple) = height;
				std::get<2>(*tuple).clear();
				std::get<3>(*tuple).clear();
				std::get<4>(*tuple).clear();
				std::get<5>(*tuple) = priority;

				auto mask = child->FirstChildElement("mask");
				while (mask) {
					auto maskPath = BSFixedString(mask->Attribute("path"));
					auto color = mask->Attribute("color");
					UInt32 colorValue;
					sscanf_s(color, "%x", &colorValue);
					auto alpha = mask->DoubleAttribute("alpha");

					std::get<2>(*tuple).push_back(maskPath.data);
					std::get<3>(*tuple).push_back(colorValue);
					std::get<4>(*tuple).push_back(alpha);

					mask = mask->NextSiblingElement("mask");
				}

				child = child->NextSiblingElement("geometry");
			}

			object = object->NextSiblingElement("object");
		}
	}
}