#include "CDXNifScene.h"
#include "CDXNifMesh.h"
#include "CDXNifBrush.h"
#include "CDXMaterial.h"
#include "CDXShader.h"

#include "skse/GameTypes.h"

#include "skse/NiRenderer.h"
#include "skse/NiTextures.h"
#include "skse/NiProperties.h"
#include "skse/NiNodes.h"

#include "skse/ScaleformLoader.h"

CDXNifScene::CDXNifScene() : CDXEditableScene()
{
	m_textureGroup = NULL;
	m_currentBrush = CDXBrush::kBrushType_Smooth;
	m_actor = NULL;

	m_width = 512;
	m_height = 512;
}

void CDXNifScene::CreateBrushes()
{
	m_brushes.push_back(new CDXNifMaskAddBrush);
	m_brushes.push_back(new CDXNifMaskSubtractBrush);
	m_brushes.push_back(new CDXNifInflateBrush);
	m_brushes.push_back(new CDXNifDeflateBrush);
	m_brushes.push_back(new CDXNifSmoothBrush);
	m_brushes.push_back(new CDXNifMoveBrush);
}

void CDXNifScene::Setup(LPDIRECT3DDEVICE9 pDevice)
{
	if (m_textureGroup)
		Release();

	CDXEditableScene::Setup(pDevice);

	BSScaleformImageLoader * imageLoader = GFxLoader::GetSingleton()->imageLoader;
	if (!imageLoader) {
		_ERROR("%s - No image loader found", __FUNCTION__);
		return;
	}
	NiTexture::FormatPrefs format;
	format.mipMapped = 2;
	format.alphaFormat = 2;
	format.pixelLayout = 6;
	BSFixedString meshTexture("headMesh");
	m_textureGroup = CreateRenderTargetGroup(&meshTexture, m_width, m_height, &format, 0, 1, 0, 0, 0, 0, 0);
	if (!m_textureGroup) {
		_ERROR("%s - Failed to create dynamic texture", __FUNCTION__);
		return;
	}

	m_textureGroup->renderedTexture[0]->name = meshTexture.data;
	CALL_MEMBER_FN(imageLoader, AddVirtualImage)((NiTexture**)&m_textureGroup->renderedTexture[0]);
}

void CDXNifScene::Release()
{
	if(m_textureGroup) {
		NiTexture * texture = m_textureGroup->renderedTexture[0];
		if(texture) {
			BSScaleformImageLoader * imageLoader = GFxLoader::GetSingleton()->imageLoader;
			UInt8 ret = CALL_MEMBER_FN(imageLoader, ReleaseVirtualImage)(&texture);
		}

		m_textureGroup->DecRef();
	}

	ReleaseImport();

	m_textureGroup = NULL;
	m_actor = NULL;

	CDXEditableScene::Release();
}

void CDXNifScene::ReleaseImport()
{
	if (m_importRoot) {
		m_importRoot->DecRef();
	}

	m_importRoot = NULL;
}