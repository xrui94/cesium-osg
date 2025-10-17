#include "Cesium3DTileset.h"
#include "AsyncTaskProcessor.h"
#include "SimpleAssetAccessor.h"
#include "SimpleRenderResourcesPreparer.h"
#include "Log.h"

#include <Cesium3DTilesContent/registerAllTileContentTypes.h>
#include <Cesium3DTilesSelection/Tileset.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include <Cesium3DTilesSelection/TilesetOptions.h>
#include <Cesium3DTilesSelection/ViewState.h>
#include <Cesium3DTilesSelection/BoundingVolume.h>
#include <Cesium3DTilesSelection/IPrepareRendererResources.h>
#include <Cesium3DTilesSelection/TileContent.h>
#include <CesiumUtility/CreditSystem.h>
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/ITaskProcessor.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumAsync/IAssetRequest.h>
#include <CesiumAsync/IAssetResponse.h>
#include <CesiumAsync/HttpHeaders.h>
#include <CesiumUtility/Uri.h>
#include <CesiumAsync/Future.h>
#include <CesiumAsync/Promise.h>
#include <CesiumGltfContent/GltfUtilities.h>
#include <CesiumGltf/Model.h>
#include <CesiumGltf/AccessorView.h>
#include <CesiumGltf/ImageAsset.h>

#include <osgUtil/CullVisitor>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


// 确保仅注册一次所有3D Tiles内容类型
static void ensureTileContentTypesRegistered()
{
	static bool s_registered = false;
	if (!s_registered) {
		Cesium3DTilesContent::registerAllTileContentTypes();
		CO_TRACE("Registered all Cesium 3D Tiles content types");
		s_registered = true;
	}
}

Cesium3DTileset::Cesium3DTileset(const std::string& url, float maximumScreenSpaceError)
	: m_tileset(nullptr)
{
	ensureTileContentTypesRegistered();
	initializeTileset(url, maximumScreenSpaceError);
	setCullingActive(false);
}

Cesium3DTileset::Cesium3DTileset(unsigned int assetID, const std::string& server, const std::string& token, float maximumScreenSpaceError)
	: m_tileset(nullptr)
{
	ensureTileContentTypesRegistered();
	initializeTileset(assetID, server, token, maximumScreenSpaceError);
	setCullingActive(false);
}

Cesium3DTileset::~Cesium3DTileset()
{
	if (m_tileset) {
		Cesium3DTilesSelection::Tileset* tileset = static_cast<Cesium3DTilesSelection::Tileset*>(m_tileset);
		delete tileset;
	}
}


float Cesium3DTileset::getMaximumScreenSpaceError() const
{
	if (m_tileset) {
		Cesium3DTilesSelection::Tileset* tileset = static_cast<Cesium3DTilesSelection::Tileset*>(m_tileset);
		return tileset->getOptions().maximumScreenSpaceError;
	}
	return 16.0f;
}

void Cesium3DTileset::setMaximumScreenSpaceError(float error)
{
	if (m_tileset) {
		Cesium3DTilesSelection::Tileset* tileset = static_cast<Cesium3DTilesSelection::Tileset*>(m_tileset);
		tileset->getOptions().maximumScreenSpaceError = error;
	}
}

bool Cesium3DTileset::getForbidHoles() const
{
	if (m_tileset) {
		Cesium3DTilesSelection::Tileset* tileset = static_cast<Cesium3DTilesSelection::Tileset*>(m_tileset);
		return tileset->getOptions().forbidHoles;
	}
	return false;
}

void Cesium3DTileset::setForbidHoles(bool forbidHoles)
{
	if (m_tileset) {
		Cesium3DTilesSelection::Tileset* tileset = static_cast<Cesium3DTilesSelection::Tileset*>(m_tileset);
		tileset->getOptions().forbidHoles = forbidHoles;
	}
}

bool Cesium3DTileset::isRootTileAvailable() const
{
	// tileset.json 是否已经经解析？
	if (m_tileset) {
		Cesium3DTilesSelection::Tileset* tileset = static_cast<Cesium3DTilesSelection::Tileset*>(m_tileset);
		return tileset->getRootTileAvailableEvent().isReady();
	}
	return false;
}

bool Cesium3DTileset::isLoaded() const
{
	if (m_tileset) {
		Cesium3DTilesSelection::Tileset* tileset = static_cast<Cesium3DTilesSelection::Tileset*>(m_tileset);
		return tileset->getRootTile() != nullptr;
	}
	return false;
}

void Cesium3DTileset::initializeTileset(const std::string& url, float maximumScreenSpaceError)
{
	// 创建必要的 Cesium 组件
	m_taskProcessor = std::make_shared<AsyncTaskProcessor>();

	// 使用同步模式确保瓦片加载在主线程中完成
	//m_taskProcessor->setSynchronous(true);
	CO_DEBUG("AsyncTaskProcessor set to synchronous mode for reliable loading");

	m_asyncSystem = std::make_shared<CesiumAsync::AsyncSystem>(m_taskProcessor);
	m_assetAccessor = std::make_shared<SimpleAssetAccessor>();
	m_prepareRenderResources = std::make_shared<SimpleRenderResourcesPreparer>();
	m_creditSystem = std::make_shared<CesiumUtility::CreditSystem>();

	// 创建 TilesetExternals
	Cesium3DTilesSelection::TilesetExternals externals{
		m_assetAccessor,
		m_prepareRenderResources,
		*m_asyncSystem,
		m_creditSystem,
		czmosg::logger(), // logger
		nullptr  // pTilesetContentManager
	};

	// 创建 TilesetOptions - 使用中等设置测试异步模式
	Cesium3DTilesSelection::TilesetOptions options;
	options.maximumScreenSpaceError = 16.0;  // 中等值，应该触发瓦片加载
	options.maximumCachedBytes = 256 * 1024 * 1024; // 256MB
	options.maximumSimultaneousTileLoads = 1;  // 限制并发加载为1
	options.loadingDescendantLimit = 1;  // 限制后代加载为1
	options.enableFrustumCulling = true;  // 启用视锥体剔除
	options.enableFogCulling = true;  // 启用雾剔除
	options.enableOcclusionCulling = true;
	options.delayRefinementForOcclusion = true;  // 启用遮挡延迟细化
	options.contentOptions.generateMissingNormalsSmooth = true;

	// 创建 Tileset
	CO_INFO("Loading Cesium 3D Tiles from URL: {}", url);
	CO_INFO("maximumScreenSpaceError: {}", options.maximumScreenSpaceError);
	m_tileset = new Cesium3DTilesSelection::Tileset(externals, url, options);

	if (isRootTileAvailable()) {
		CO_INFO("Root tile is immediately available after tileset creation");
	}
	else {
		CO_INFO("Root tile not yet available, waiting for loading...");
	}

	CO_DEBUG("Async mode enabled to test thread safety");
}

void Cesium3DTileset::initializeTileset(unsigned int assetID, const std::string& server, const std::string& token, float maximumScreenSpaceError)
{
	// 创建必要的 Cesium 组件
	m_taskProcessor = std::make_shared<AsyncTaskProcessor>();
	m_assetAccessor = std::make_shared<SimpleAssetAccessor>();
	m_prepareRenderResources = std::make_shared<SimpleRenderResourcesPreparer>();
	m_creditSystem = std::make_shared<CesiumUtility::CreditSystem>();

	// 创建 TilesetExternals
	Cesium3DTilesSelection::TilesetExternals externals{
		m_assetAccessor,
		m_prepareRenderResources,
		*m_asyncSystem,
		m_creditSystem,
		czmosg::logger(), // logger
		nullptr  // pTilesetContentManager
	};

	// 创建 TilesetOptions
	Cesium3DTilesSelection::TilesetOptions options;
	options.maximumScreenSpaceError = maximumScreenSpaceError;
	options.contentOptions.generateMissingNormalsSmooth = true;

	// 创建 Tileset（使用 Asset ID）
	CO_INFO("Loading Cesium 3D Tiles from Asset ID: {}", assetID);
	CO_INFO("maximumScreenSpaceError: {}", options.maximumScreenSpaceError);
	m_tileset = new Cesium3DTilesSelection::Tileset(externals, assetID, token, options, server);
}

osg::BoundingSphere Cesium3DTileset::computeBound() const
{
	if (m_tileset) {
		Cesium3DTilesSelection::Tileset* tileset = static_cast<Cesium3DTilesSelection::Tileset*>(m_tileset);

		// 检查根瓦片是否已经可用
		if (!tileset->getRootTileAvailableEvent().isReady()) {
			CO_TRACE("computeBound - Root tile not yet available, tileset still loading");
			return osg::BoundingSphere();
		}

		auto rootTile = tileset->getRootTile();
		if (rootTile) {
			auto bbox = Cesium3DTilesSelection::getOrientedBoundingBoxFromBoundingVolume(rootTile->getBoundingVolume());
			auto& center = bbox.getCenter();
			auto& lengths = bbox.getLengths();
			float radius = std::max(lengths.x, std::max(lengths.y, lengths.z));
			CO_TRACE("computeBound - Center: ({}, {}, {})", center.x, center.y, center.z);
			CO_TRACE("computeBound - Lengths: ({}, {}, {})", lengths.x, lengths.y, lengths.z);
			CO_TRACE("computeBound - Radius: {}", radius);
			return osg::BoundingSphere(osg::Vec3(center.x, center.y, center.z), radius);
		}
		else {
			CO_ERROR("computeBound - Root tile available event is ready but no root tile found");
		}
	}
	else {
		CO_ERROR("computeBound - No tileset available");
	}
	return osg::BoundingSphere();
}

void Cesium3DTileset::traverse(osg::NodeVisitor& nv)
{
	if (!m_tileset) {
		osg::Group::traverse(nv);
		return;
	}

	//// 检查根瓦片加载失败
	//if (isRootTileAvailable()) {
	//	if (!isLoaded()) {
	//		CO_CRITICAL("Tileset load failed: root tile unavailable. Please check your URL or network.");
	//		m_tilesetFailed = true; // 标记为失败，后续不再尝试
	//		return;
	//	}
	//}
	//else {
	//	// 还在加载中，可以输出一次等待日志（可选）
	//	if (!m_waitingLogged) {
	//		CO_INFO("Waiting for root tile to become available...");
	//		m_waitingLogged = true;
	//		return;
	//	}
	//}

	static int frameCount = 0;

	if (nv.getVisitorType() == osg::NodeVisitor::CULL_VISITOR) {
		Cesium3DTilesSelection::Tileset* tileset = static_cast<Cesium3DTilesSelection::Tileset*>(m_tileset);
		//// 检查根瓦片状态
		//auto rootTile = tileset->getRootTile();
		//if (rootTile) {
		//	auto tileState = rootTile->getState();
		//	if (tileState != Cesium3DTilesSelection::TileLoadState::Done &&
		//		tileState != Cesium3DTilesSelection::TileLoadState::ContentLoaded
		//		) {
		//		CO_CRITICAL("Tileset root tile load failed, will not retry. Please check your URL or network.");
		//		return;
		//	}
		//	CO_DEBUG("Root tile not ready for rendering, state: {}", static_cast<int>(tileState));
		//}
		//else {
		//	CO_DEBUG("Root tile not yet available for state check");
		//}

		//
		frameCount++;
		CO_TRACE("Cesium3DTileset::traverse called (frame {})", frameCount);

		const osgUtil::CullVisitor* cv = nv.asCullVisitor();
		if (!cv) {
			osg::Group::traverse(nv);
			return;
		}

		// 参考osgEarth实现：从 osg::NodeVisitor 实例动态计算相机参数
		osg::Vec3d osgEye, osgCenter, osgUp;
		cv->getModelViewMatrix()->getLookAt(osgEye, osgCenter, osgUp);
		osg::Vec3d osgDir = osgCenter - osgEye;
		osgDir.normalize();

		glm::dvec3 position(osgEye.x(), osgEye.y(), osgEye.z());
		glm::dvec3 direction(osgDir.x(), osgDir.y(), osgDir.z());
		glm::dvec3 up(osgUp.x(), osgUp.y(), osgUp.z());
		glm::dvec2 viewportSize(cv->getViewport()->width(), cv->getViewport()->height());

		double vfov, ar, znear, zfar;
		cv->getProjectionMatrix()->getPerspective(vfov, ar, znear, zfar);
		vfov = osg::DegreesToRadians(vfov);
		double hfov = 2 * atan(tan(vfov / 2) * (ar));

		// 调试输出相机参数
		CO_TRACE("Camera position: ({}, {}, {})", position.x, position.y, position.z);
		CO_TRACE("Camera direction: ({}, {}, {})", direction.x, direction.y, direction.z);
		CO_TRACE("Viewport size: ({}, {})", viewportSize.x, viewportSize.y);
		CO_TRACE("FOV: hfov={}, vfov={}", glm::degrees(hfov), glm::degrees(vfov));

		Cesium3DTilesSelection::ViewState viewState = Cesium3DTilesSelection::ViewState::create(
			position, direction, up, viewportSize, hfov, vfov
		);
		std::vector<Cesium3DTilesSelection::ViewState> viewStateList = { viewState };
		auto updateResult = tileset->updateView(viewStateList);

		// 处理异步任务 - 在同步模式下这很重要
		tileset->loadTiles();

		CO_TRACE("Tiles to render this frame: {}", updateResult.tilesToRenderThisFrame.size());
		CO_TRACE("Worker thread load queue: {}", updateResult.workerThreadTileLoadQueueLength);
		CO_TRACE("Main thread load queue: {}", updateResult.mainThreadTileLoadQueueLength);

		// 清除之前的子节点
		removeChildren(0, getNumChildren());

		// 添加要渲染的瓦片
		for (const auto& tile : updateResult.tilesToRenderThisFrame) {
			if (tile->getContent().isRenderContent()) {
				MainThreadResult* result = reinterpret_cast<MainThreadResult*>(tile->getContent().getRenderContent()->getRenderResources());
				if (result && result->node.valid()) {
					CO_TRACE("Adding tile to scene graph");
					addChild(result->node.get());
				}
			}
			//else {
			//	const auto& content = tile->getContent();
			//	std::cout << "Tile content - isRenderContent: " << content.isRenderContent()
			//		<< ", isEmptyContent: " << content.isEmptyContent()
			//		<< ", isExternalContent: " << content.isExternalContent() << std::endl;

			//	// 如果是空内容，检查子瓦片
			//	if (content.isEmptyContent()) {
			//		std::cout << "Empty tile, checking children..." << std::endl;
			//		// 强制加载子瓦片
			//		auto children = tile->getChildren();
			//		std::cout << "Number of children: " << children.size() << std::endl;
			//		for (const auto& child : children) {
			//			std::cout << "Child tile state: " << static_cast<int>(child.getState()) << std::endl;
			//		}
			//	}
			//}
		}
	}
	else if (nv.getVisitorType() == osg::NodeVisitor::UPDATE_VISITOR) {
		// 在UPDATE_VISITOR中不做瓦片更新，只调用基类
		osg::Group::traverse(nv);
		return;
	}

	osg::Group::traverse(nv);
}
