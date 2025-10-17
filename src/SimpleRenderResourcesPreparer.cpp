#include "SimpleRenderResourcesPreparer.h"
#include "Log.h"

#include <CesiumGltfContent/GltfUtilities.h>
#include <CesiumGltf/AccessorView.h>

#include <osg/Geode>
#include <osg/Texture2D>
#include <osg/Geometry>
#include <osg/Notify>
#include <osg/MatrixTransform>
#include <osg/Material>
#include <osg/StateAttribute>

#include <glm/gtc/type_ptr.hpp>


//static bool startsWith(const std::string& str, const std::string& prefix)
//{
//    if (prefix.size() > str.size()) {
//        return false;
//    }
//    return str.compare(0, prefix.size(), prefix) == 0;
//}

// 重载版本：支持 C 风格字符串（char*）
//static bool startsWith(const std::string& str, const char* prefix)
//{
//    if (!prefix) return false;
//    std::string prefixStr(prefix);
//    if (prefixStr.size() > str.size()) {
//        return false;
//    }
//    return str.compare(0, prefixStr.size(), prefixStr) == 0;
//}

/**
* Material that will work in both FFP and non-FFP mode, by using the uniform
* osg_FrontMaterial in place of gl_FrontMaterial.
*/
class MaterialGL3 : public osg::Material
{
public:
    MaterialGL3() : osg::Material() {}
    MaterialGL3(const MaterialGL3& mat, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY) : osg::Material(mat, copyop) {}
    MaterialGL3(const osg::Material& mat) : osg::Material(mat) {}

    META_StateAttribute(osgEarth, MaterialGL3, MATERIAL);

    void apply(osg::State& state) const override
    {
#ifdef OSG_GL_FIXED_FUNCTION_AVAILABLE
        osg::Material::apply(state);
#else
        state.Color(_diffuseFront.r(), _diffuseFront.g(), _diffuseFront.b(), _diffuseFront.a());
#endif
    }
};

LoadThreadResult::LoadThreadResult()
{
}

LoadThreadResult::~LoadThreadResult()
{    
}

MainThreadResult::MainThreadResult()
{
}

MainThreadResult::~MainThreadResult()
{
}


LoadRasterThreadResult::LoadRasterThreadResult()
{
}

LoadRasterThreadResult::~LoadRasterThreadResult()
{
}

LoadRasterMainThreadResult::LoadRasterMainThreadResult()
{
}

LoadRasterMainThreadResult::~LoadRasterMainThreadResult()
{
}


namespace {

	// 将glTF AccessorView转换为OSG数组的模板函数
	template<typename T>
	osg::Array* accessorViewToArray(const CesiumGltf::AccessorView<T>& accessorView) {
		return nullptr;
	}

	template<>
	osg::Array* accessorViewToArray(const CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC3<float>>& accessorView) {
		osg::Vec3Array* result = new osg::Vec3Array(static_cast<unsigned int>(accessorView.size()));
		for (unsigned int i = 0; i < static_cast<unsigned int>(accessorView.size()); ++i) {
			auto& data = accessorView[i];
			(*result)[static_cast<unsigned int>(i)] = osg::Vec3(data.value[0], data.value[1], data.value[2]);
		}
		return result;
	}

	template<>
	osg::Array* accessorViewToArray(const CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC2<float>>& accessorView) {
		osg::Vec2Array* result = new osg::Vec2Array(static_cast<unsigned int>(accessorView.size()));
		for (unsigned int i = 0; i < static_cast<unsigned int>(accessorView.size()); ++i) {
			auto& data = accessorView[i];
			(*result)[i] = osg::Vec2(data.value[0], data.value[1]);
		}
		return result;
	}

	template<>
	osg::Array* accessorViewToArray(const CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::SCALAR<uint16_t>>& accessorView) {
		osg::UShortArray* result = new osg::UShortArray(static_cast<unsigned int>(accessorView.size()));
		for (unsigned int i = 0; i < static_cast<unsigned int>(accessorView.size()); ++i) {
			auto& data = accessorView[i];
			(*result)[i] = data.value[0];
		}
		return result;
	}

	template<>
	osg::Array* accessorViewToArray(const CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::SCALAR<uint32_t>>& accessorView) {
		osg::UIntArray* result = new osg::UIntArray(static_cast<unsigned int>(accessorView.size()));
		for (unsigned int i = 0; i < static_cast<unsigned int>(accessorView.size()); ++i) {
			auto& data = accessorView[i];
			(*result)[i] = data.value[0];
		}
		return result;
	}

	// NodeBuilder类：将glTF模型转换为OSG节点
	class NodeBuilder {
	public:
		NodeBuilder(CesiumGltf::Model* model, const glm::dmat4& transform)
			: _model(model), _transform(transform) {
		}

		osg::Node* build() {
			osg::MatrixTransform* root = new osg::MatrixTransform;
			osg::Matrixd matrix;

			glm::dmat4x4 rootTransform = _transform;
			rootTransform = CesiumGltfContent::GltfUtilities::applyRtcCenter(*_model, rootTransform);
			rootTransform = CesiumGltfContent::GltfUtilities::applyGltfUpAxisTransform(*_model, rootTransform);
			matrix.set(glm::value_ptr(rootTransform));

			root->setMatrix(matrix);

			if (!_model->scenes.empty()) {
				for (auto& scene : _model->scenes) {
					for (int node : scene.nodes) {
						root->addChild(createNode(_model->nodes[node]));
					}
				}
			}
			else {
				for (auto itr = _model->nodes.begin(); itr != _model->nodes.end(); ++itr) {
					root->addChild(createNode(*itr));
				}
			}

			osg::Group* container = new osg::Group;
			container->addChild(root);
			return container;
		}

	private:
		osg::Node* createNode(const CesiumGltf::Node& node) {
			osg::MatrixTransform* root = new osg::MatrixTransform;

			if (node.matrix.size() == 16) {
				osg::Matrixd matrix;
				matrix.set(node.matrix.data());
				root->setMatrix(matrix);
			}

			if (root->getMatrix().isIdentity()) {
				osg::Matrixd scale, translation, rotation;
				if (node.scale.size() == 3) {
					scale = osg::Matrixd::scale(node.scale[0], node.scale[1], node.scale[2]);
				}

				if (node.rotation.size() == 4) {
					osg::Quat quat(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
					rotation.makeRotate(quat);
				}

				if (node.translation.size() == 3) {
					translation = osg::Matrixd::translate(node.translation[0], node.translation[1], node.translation[2]);
				}

				root->setMatrix(scale * rotation * translation);
			}

			if (node.mesh >= 0) {
				root->addChild(createMesh(_model->meshes[node.mesh]));
			}

			for (int child : node.children) {
				root->addChild(createNode(_model->nodes[child]));
			}

			return root;
		}

		osg::Node* createMesh(const CesiumGltf::Mesh& mesh) {
			osg::Group* group = new osg::Group;

			CO_TRACE("Creating mesh with {} primitives", mesh.primitives.size());

			for (const auto& primitive : mesh.primitives) {
				osg::Geometry* geometry = new osg::Geometry;

				// 处理顶点属性
				for (const auto& attribute : primitive.attributes) {
					const std::string& attributeName = attribute.first;
					int accessorIndex = attribute.second;

					if (accessorIndex < 0 || accessorIndex >= _model->accessors.size()) {
						continue;
					}

					const auto& accessor = _model->accessors[accessorIndex];

					if (attributeName == "POSITION") {
						auto positionView = CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC3<float>>(*_model, accessor);
						if (positionView.status() == CesiumGltf::AccessorViewStatus::Valid) {
							osg::Array* vertices = accessorViewToArray(positionView);
							if (vertices) {
								geometry->setVertexArray(vertices);
							}
						}
					}
					else if (attributeName == "NORMAL") {
						auto normalView = CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC3<float>>(*_model, accessor);
						if (normalView.status() == CesiumGltf::AccessorViewStatus::Valid) {
							osg::Array* normals = accessorViewToArray(normalView);
							if (normals) {
								geometry->setNormalArray(normals);
								geometry->setNormalBinding(osg::Geometry::BIND_PER_VERTEX);
							}
						}
					}
					else if (attributeName == "TEXCOORD_0") {
						auto texCoordView = CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC2<float>>(*_model, accessor);
						if (texCoordView.status() == CesiumGltf::AccessorViewStatus::Valid) {
							osg::Array* texCoords = accessorViewToArray(texCoordView);
							if (texCoords) {
								geometry->setTexCoordArray(0, texCoords);
							}
						}
					}
				}

				// 处理索引
				if (primitive.indices >= 0 && primitive.indices < _model->accessors.size()) {
					const auto& indexAccessor = _model->accessors[primitive.indices];

					if (indexAccessor.componentType == CesiumGltf::Accessor::ComponentType::UNSIGNED_SHORT) {
						auto indexView = CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::SCALAR<uint16_t>>(*_model, indexAccessor);
						if (indexView.status() == CesiumGltf::AccessorViewStatus::Valid) {
							osg::UShortArray* indices = static_cast<osg::UShortArray*>(accessorViewToArray(indexView));
							if (indices) {
								osg::DrawElementsUShort* drawElements = new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES);
								for (size_t i = 0; i < indices->size(); ++i) {
									drawElements->push_back((*indices)[i]);
								}
								geometry->addPrimitiveSet(drawElements);
							}
						}
					}
					else if (indexAccessor.componentType == CesiumGltf::Accessor::ComponentType::UNSIGNED_INT) {
						auto indexView = CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::SCALAR<uint32_t>>(*_model, indexAccessor);
						if (indexView.status() == CesiumGltf::AccessorViewStatus::Valid) {
							osg::UIntArray* indices = static_cast<osg::UIntArray*>(accessorViewToArray(indexView));
							if (indices) {
								osg::DrawElementsUInt* drawElements = new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
								for (size_t i = 0; i < indices->size(); ++i) {
									drawElements->push_back((*indices)[i]);
								}
								geometry->addPrimitiveSet(drawElements);
							}
						}
					}
				}
				else {
					// 没有索引，使用顶点数组
					if (geometry->getVertexArray()) {
						geometry->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, geometry->getVertexArray()->getNumElements()));
					}
				}

				// 处理材质和纹理
				osg::StateSet* stateSet = geometry->getOrCreateStateSet();

				// 检查primitive是否有材质
				if (primitive.material >= 0 && primitive.material < _model->materials.size()) {
					const auto& material = _model->materials[primitive.material];
					CO_TRACE("Processing material {}", primitive.material);
					// 处理PBR材质
					if (material.pbrMetallicRoughness) {
						const auto& pbr = *material.pbrMetallicRoughness;

						// 设置基础颜色
						if (pbr.baseColorFactor.size() >= 4) {
							osg::Vec4 baseColor(
								pbr.baseColorFactor[0],
								pbr.baseColorFactor[1],
								pbr.baseColorFactor[2],
								pbr.baseColorFactor[3]
							);
							CO_TRACE("Base color: ({}, {}, {}, {})", baseColor.r(), baseColor.g(), baseColor.b(), baseColor.a());

							osg::Material* osgMaterial = new osg::Material;
							osgMaterial->setDiffuse(osg::Material::FRONT_AND_BACK, baseColor);
							osgMaterial->setAmbient(osg::Material::FRONT_AND_BACK, baseColor * 0.2f);
							stateSet->setAttributeAndModes(osgMaterial, osg::StateAttribute::ON);
						}

						// 处理基础颜色纹理
						if (pbr.baseColorTexture && pbr.baseColorTexture->index >= 0 && pbr.baseColorTexture->index < _model->textures.size()) {
							const auto& texture = _model->textures[pbr.baseColorTexture->index];
							CO_TRACE("Loading base color texture {}", pbr.baseColorTexture->index);

							if (texture.source >= 0 && texture.source < _model->images.size()) {
								const auto& image = _model->images[texture.source];

								// 检查图像资产是否存在
								if (image.pAsset && !image.pAsset->pixelData.empty()) {
									const auto& imageAsset = *image.pAsset;
									CO_TRACE("Image source {} , data size: {}", texture.source, imageAsset.pixelData.size());

									osg::Texture2D* osgTexture = new osg::Texture2D;

									// 创建OSG图像
									osg::Image* osgImage = new osg::Image;

									// 确定图像格式
									GLenum pixelFormat = GL_RGB;
									GLenum dataType = GL_UNSIGNED_BYTE;
									GLint internalFormat = GL_RGB8;

									if (imageAsset.channels == 4) {
										pixelFormat = GL_RGBA;
										internalFormat = GL_RGBA8;
									}
									else if (imageAsset.channels == 3) {
										pixelFormat = GL_RGB;
										internalFormat = GL_RGB8;
									}
									CO_TRACE("Image format: {}x{}, channels: {}", imageAsset.width, imageAsset.height, imageAsset.channels);

									// 复制像素数据
									unsigned char* imageData = new unsigned char[imageAsset.pixelData.size()];
									std::memcpy(imageData, imageAsset.pixelData.data(), imageAsset.pixelData.size());

									osgImage->setImage(
										imageAsset.width,
										imageAsset.height,
										1, // depth
										internalFormat,
										pixelFormat,
										dataType,
										imageData,
										osg::Image::USE_NEW_DELETE
									);

									osgTexture->setImage(osgImage);
									osgTexture->setWrap(osg::Texture2D::WRAP_S, osg::Texture2D::REPEAT);
									osgTexture->setWrap(osg::Texture2D::WRAP_T, osg::Texture2D::REPEAT);
									osgTexture->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR_MIPMAP_LINEAR);
									osgTexture->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);

									stateSet->setTextureAttributeAndModes(0, osgTexture, osg::StateAttribute::ON);
									CO_TRACE("Texture applied successfully");
								}
							}
						}
					}
				}
				else {
					CO_WARN("No material found for primitive, using default white material");
					// 设置默认白色材质
					osg::Material* defaultMaterial = new osg::Material;
					defaultMaterial->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
					defaultMaterial->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.2f, 0.2f, 0.2f, 1.0f));
					stateSet->setAttributeAndModes(defaultMaterial, osg::StateAttribute::ON);
				}

				osg::Geode* geode = new osg::Geode;
				geode->addDrawable(geometry);
				group->addChild(geode);
			}

			return group;
		}

		CesiumGltf::Model* _model;
		glm::dmat4 _transform;
	};
}


// ============================== SimpleRenderResourcesPreparer 类实现 ==============================

CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources>
	SimpleRenderResourcesPreparer::prepareInLoadThread(
		const CesiumAsync::AsyncSystem& asyncSystem,
		Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
		const glm::dmat4& transform,
		const std::any& rendererOptions)
{

	CO_DEBUG("Preparing render resources in load thread");
	CO_DEBUG("TileLoadResult state: {}", static_cast<int>(tileLoadResult.state));

	// 检查加载状态
	if (tileLoadResult.state != Cesium3DTilesSelection::TileLoadResultState::Success) {
		CO_ERROR("Tile load failed, state: {}", static_cast<int>(tileLoadResult.state));
		return asyncSystem.createResolvedFuture(
			Cesium3DTilesSelection::TileLoadResultAndRenderResources{
				std::move(tileLoadResult),
				nullptr
			});
	}

	CesiumGltf::Model* model = std::get_if<CesiumGltf::Model>(&tileLoadResult.contentKind);
	if (!model) {
		CO_ERROR("No glTF model found in tile content, contentKind index: {}", tileLoadResult.contentKind.index());
		return asyncSystem.createResolvedFuture(
			Cesium3DTilesSelection::TileLoadResultAndRenderResources{
				std::move(tileLoadResult),
				nullptr
			});
	}

	CO_TRACE("Found glTF model with {} meshes", model->meshes.size());

	NodeBuilder builder(model, transform);
	::LoadThreadResult* result = new ::LoadThreadResult;
	result->node = builder.build();

	if (!result->node.valid()) {
		CO_ERROR("Failed to build OSG node from glTF model");
		delete result;
		return asyncSystem.createResolvedFuture(
			Cesium3DTilesSelection::TileLoadResultAndRenderResources{
				std::move(tileLoadResult),
				nullptr
			});
	}
	CO_DEBUG("Successfully built OSG node");

	return asyncSystem.createResolvedFuture(
		Cesium3DTilesSelection::TileLoadResultAndRenderResources{
			std::move(tileLoadResult),
			result
		});
}

void* SimpleRenderResourcesPreparer::prepareInMainThread(
	Cesium3DTilesSelection::Tile& tile,
	void* pLoadThreadResult)
{

	CO_DEBUG("Preparing render resources in main thread");

	::LoadThreadResult* loadThreadResult = reinterpret_cast<::LoadThreadResult*>(pLoadThreadResult);
	::MainThreadResult* mainThreadResult = new ::MainThreadResult();
	mainThreadResult->node = loadThreadResult->node;

	loadThreadResult->node = nullptr;
	delete loadThreadResult;

	return mainThreadResult;
}

void SimpleRenderResourcesPreparer::free(
	Cesium3DTilesSelection::Tile& tile,
	void* pLoadThreadResult,
	void* pMainThreadResult) noexcept
{

	::LoadThreadResult* loadThreadResult = reinterpret_cast<::LoadThreadResult*>(pLoadThreadResult);
	if (loadThreadResult) {
		delete loadThreadResult;
	}

	::MainThreadResult* mainThreadResult = reinterpret_cast<::MainThreadResult*>(pMainThreadResult);
	if (mainThreadResult) {
		delete mainThreadResult;
	}
}

void* SimpleRenderResourcesPreparer::prepareRasterInLoadThread(
	CesiumGltf::ImageAsset& image,
	const std::any& rendererOptions)
{
	// 暂时不实现栅格叠加层功能
	return nullptr;
}

void* SimpleRenderResourcesPreparer::prepareRasterInMainThread(
	CesiumRasterOverlays::RasterOverlayTile& rasterTile,
	void* pLoadThreadResult)
{
	// 暂时不实现栅格叠加层功能
	return nullptr;
}

void SimpleRenderResourcesPreparer::attachRasterInMainThread(
	const Cesium3DTilesSelection::Tile& tile,
	int32_t overlayTextureCoordinateID,
	const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
	void* pMainThreadRendererResources,
	const glm::dvec2& translation,
	const glm::dvec2& scale)
{
	// 暂时不实现栅格叠加层功能
}

void SimpleRenderResourcesPreparer::detachRasterInMainThread(
	const Cesium3DTilesSelection::Tile& tile,
	int32_t overlayTextureCoordinateID,
	const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
	void* pMainThreadRendererResources) noexcept
{
	// 暂时不实现栅格叠加层功能
}

void SimpleRenderResourcesPreparer::freeRaster(
	const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
	void* pLoadThreadResult,
	void* pMainThreadResult) noexcept
{
	// 暂时不实现栅格叠加层功能
}
