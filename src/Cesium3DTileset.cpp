#include "Cesium3DTileset.h"
#include "AsyncTaskProcessor.h"
#include "SimpleAssetAccessor.h"
#include "SimpleRenderResourcesPreparer.h"

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
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <iostream>


// 确保仅注册一次所有3D Tiles内容类型
static void ensureTileContentTypesRegistered()
{
	static bool s_registered = false;
	if (!s_registered) {
		Cesium3DTilesContent::registerAllTileContentTypes();
		std::cout << "Registered all Cesium 3D Tiles content types" << std::endl;
		s_registered = true;
	}
}

Cesium3DTileset::Cesium3DTileset(const std::string& url, float maximumScreenSpaceError)
	: m_tileset(nullptr) {
	ensureTileContentTypesRegistered();
	initializeTileset(url, maximumScreenSpaceError);
	setCullingActive(false);
}

Cesium3DTileset::Cesium3DTileset(unsigned int assetID, const std::string& server, const std::string& token, float maximumScreenSpaceError)
	: m_tileset(nullptr) {
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

void Cesium3DTileset::traverse(osg::NodeVisitor& nv)
{
	if (!m_tileset) {
		osg::Group::traverse(nv);
		return;
	}

	static int frameCount = 0;

	if (nv.getVisitorType() == osg::NodeVisitor::CULL_VISITOR) {
		frameCount++;
		std::cout << "Cesium3DTileset::traverse called (frame " << frameCount << ")" << std::endl;

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
		std::cout << "Camera position: (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
		std::cout << "Camera direction: (" << direction.x << ", " << direction.y << ", " << direction.z << ")" << std::endl;
		std::cout << "Viewport size: (" << viewportSize.x << ", " << viewportSize.y << ")" << std::endl;
		std::cout << "FOV: hfov=" << glm::degrees(hfov) << ", vfov=" << glm::degrees(vfov) << std::endl;

		Cesium3DTilesSelection::ViewState viewState = Cesium3DTilesSelection::ViewState::create(
			position, direction, up, viewportSize, hfov, vfov
		);

		std::vector<Cesium3DTilesSelection::ViewState> viewStates = { viewState };

		Cesium3DTilesSelection::Tileset* tileset = static_cast<Cesium3DTilesSelection::Tileset*>(m_tileset);
		auto updateResult = tileset->updateView(viewStates);

		// 处理异步任务 - 在同步模式下这很重要
		tileset->loadTiles();

		std::cout << "Tiles to render this frame: " << updateResult.tilesToRenderThisFrame.size() << std::endl;
		std::cout << "Worker thread load queue: " << updateResult.workerThreadTileLoadQueueLength << std::endl;
		std::cout << "Main thread load queue: " << updateResult.mainThreadTileLoadQueueLength << std::endl;

		// 检查根瓦片状态
		auto rootTile = tileset->getRootTile();
		if (rootTile) {
			std::cout << "Root tile state: " << static_cast<int>(rootTile->getState()) << std::endl;
		}
		else {
			std::cout << "No root tile available yet" << std::endl;
		}

		// 清除之前的子节点
		removeChildren(0, getNumChildren());

		// 添加要渲染的瓦片
		for (const auto& tile : updateResult.tilesToRenderThisFrame) {
			if (tile->getContent().isRenderContent()) {
				MainThreadResult* result = reinterpret_cast<MainThreadResult*>(tile->getContent().getRenderContent()->getRenderResources());
				if (result && result->node.valid()) {
					std::cout << "Adding tile to scene graph" << std::endl;
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

osg::BoundingSphere Cesium3DTileset::computeBound() const
{
	if (m_tileset) {
		Cesium3DTilesSelection::Tileset* tileset = static_cast<Cesium3DTilesSelection::Tileset*>(m_tileset);

		// 检查根瓦片是否已经可用
		if (!tileset->getRootTileAvailableEvent().isReady()) {
			std::cout << "computeBound - Root tile not yet available, tileset still loading" << std::endl;
			return osg::BoundingSphere();
		}

		auto rootTile = tileset->getRootTile();
		if (rootTile) {
			auto bbox = Cesium3DTilesSelection::getOrientedBoundingBoxFromBoundingVolume(rootTile->getBoundingVolume());
			auto& center = bbox.getCenter();
			auto& lengths = bbox.getLengths();
			float radius = std::max(lengths.x, std::max(lengths.y, lengths.z));
			std::cout << "computeBound - Center: (" << center.x << ", " << center.y << ", " << center.z << ")" << std::endl;
			std::cout << "computeBound - Lengths: (" << lengths.x << ", " << lengths.y << ", " << lengths.z << ")" << std::endl;
			std::cout << "computeBound - Radius: " << radius << std::endl;
			return osg::BoundingSphere(osg::Vec3(center.x, center.y, center.z), radius);
		}
		else {
			std::cout << "computeBound - Root tile available event is ready but no root tile found" << std::endl;
		}
	}
	else {
		std::cout << "computeBound - No tileset available" << std::endl;
	}
	return osg::BoundingSphere();
}

void Cesium3DTileset::initializeTileset(const std::string& url, float maximumScreenSpaceError)
{
	// 创建必要的 Cesium 组件
	m_taskProcessor = std::make_shared<AsyncTaskProcessor>();

	// 使用同步模式确保瓦片加载在主线程中完成
	m_taskProcessor->setSynchronous(true);
	std::cout << "AsyncTaskProcessor set to synchronous mode for reliable loading" << std::endl;

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
		nullptr, // logger
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
	m_tileset = new Cesium3DTilesSelection::Tileset(externals, url, options);

	std::cout << "Cesium3DTileset initialized with URL: " << url << std::endl;
	std::cout << "maximumScreenSpaceError: " << options.maximumScreenSpaceError << std::endl;
	std::cout << "Async mode enabled to test thread safety" << std::endl;
}

void Cesium3DTileset::initializeTileset(unsigned int assetID, const std::string& server, const std::string& token, float maximumScreenSpaceError)
{
	// 创建必要的 Cesium 组件
	m_taskProcessor = std::make_shared<AsyncTaskProcessor>();
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
		nullptr, // logger
		nullptr  // pTilesetContentManager
	};

	// 创建 TilesetOptions
	Cesium3DTilesSelection::TilesetOptions options;
	options.maximumScreenSpaceError = maximumScreenSpaceError;
	options.contentOptions.generateMissingNormalsSmooth = true;

	// 创建 Tileset（使用 Asset ID）
	m_tileset = new Cesium3DTilesSelection::Tileset(externals, assetID, token, options, server);

	std::cout << "Cesium3DTileset initialized with Asset ID: " << assetID << std::endl;
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
	if (m_tileset) {
		Cesium3DTilesSelection::Tileset* tileset = static_cast<Cesium3DTilesSelection::Tileset*>(m_tileset);
		return tileset->getRootTileAvailableEvent().isReady();
	}
	return false;
}
