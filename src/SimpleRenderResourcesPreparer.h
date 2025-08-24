#pragma once

// Include Material.h to avoid issues with OPAQUE definition from Windows.h when Tileset.h is included
#include <CesiumGltf/Material.h>
#include <Cesium3DTilesSelection/Tileset.h>

#include <osg/Node>



class LoadThreadResult
{
public:
	LoadThreadResult();
	~LoadThreadResult();
	osg::ref_ptr< osg::Node > node;
};

class MainThreadResult
{
public:
	MainThreadResult();
	~MainThreadResult();
	osg::ref_ptr< osg::Node > node;
};


class LoadRasterThreadResult
{
public:
	LoadRasterThreadResult();
	~LoadRasterThreadResult();
	osg::ref_ptr< osg::Image > image;
};

class LoadRasterMainThreadResult
{
public:
	LoadRasterMainThreadResult();
	~LoadRasterMainThreadResult();
	osg::ref_ptr< osg::Image > image;
};


class SimpleRenderResourcesPreparer: public Cesium3DTilesSelection::IPrepareRendererResources
{
public:
	CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources>
		prepareInLoadThread(
			const CesiumAsync::AsyncSystem& asyncSystem,
			Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
			const glm::dmat4& transform,
			const std::any& rendererOptions) override;

	void* prepareInMainThread(
		Cesium3DTilesSelection::Tile& tile,
		void* pLoadThreadResult) override;

	void free(
		Cesium3DTilesSelection::Tile& tile,
		void* pLoadThreadResult,
		void* pMainThreadResult) noexcept;

	void* prepareRasterInLoadThread(
		CesiumGltf::ImageAsset& image,
		const std::any& rendererOptions) override;

	void* prepareRasterInMainThread(
		CesiumRasterOverlays::RasterOverlayTile& rasterTile,
		void* pLoadThreadResult) override;

	void attachRasterInMainThread(
		const Cesium3DTilesSelection::Tile& tile,
		int32_t overlayTextureCoordinateID,
		const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
		void* pMainThreadRendererResources,
		const glm::dvec2& translation,
		const glm::dvec2& scale) override;

	void detachRasterInMainThread(
		const Cesium3DTilesSelection::Tile& tile,
		int32_t overlayTextureCoordinateID,
		const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
		void* pMainThreadRendererResources) noexcept override;

	void freeRaster(
		const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
		void* pLoadThreadResult,
		void* pMainThreadResult) noexcept override;
};