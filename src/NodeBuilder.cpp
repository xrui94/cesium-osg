#include "NodeBuilder.h"
#include "Log.h"

#include <glm/gtc/type_ptr.hpp>

#include <CesiumGltfContent/GltfUtilities.h>
#include <CesiumGltf/AccessorView.h>

#include <osg/Geode>
#include <osg/Texture2D>
#include <osg/Geometry>
#include <osg/Notify>
#include <osg/MatrixTransform>
#include <osg/Material>
#include <osg/StateAttribute>

#include <cmath>

namespace czmosg
{
    template<typename T>
    inline T clamp(const T& v, const T& lo, const T& hi) {
        return (v < lo) ? lo : (hi < v) ? hi : v;
    }

    // 辅助函数：Gamma 校正（线性 → sRGB/Gamma 空间）
    inline osg::Vec4 gammaCorrectLinearToSRGB(const osg::Vec4& linearColor, float gamma = 2.2f) {
        if (gamma <= 0.0f) gamma = 2.2f;
        return osg::Vec4(
            std::pow(std::max(linearColor.r(), 0.0f), 1.0f / gamma),
            std::pow(std::max(linearColor.g(), 0.0f), 1.0f / gamma),
            std::pow(std::max(linearColor.b(), 0.0f), 1.0f / gamma),
            linearColor.a() // Alpha 不校正
        );
    }

    // 通用 Gamma 校正（自定义 gamma，用于调试/特殊管线）
    inline osg::Vec4 linearToGammaSpace(const osg::Vec4& linearColor, float gamma = 2.2f) {
        if (gamma <= 0.0f) gamma = 2.2f;
        return osg::Vec4(
            std::pow(clamp(linearColor.r(), 0.0f, 1.0f), 1.0f / gamma),
            std::pow(clamp(linearColor.g(), 0.0f, 1.0f), 1.0f / gamma),
            std::pow(clamp(linearColor.b(), 0.0f, 1.0f), 1.0f / gamma),
            linearColor.a()
        );
    }

    // 标准 sRGB 编码（符合 glTF 规范，推荐默认使用）
    inline osg::Vec4 linearToSRGB(const osg::Vec4& linearColor) {
        auto encode = [](float c) -> float {
            c = clamp(c, 0.0f, 1.0f);
            return (c <= 0.0031308f) ?
                c * 12.92f :
                1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
            };
        return osg::Vec4(
            encode(linearColor.r()),
            encode(linearColor.g()),
            encode(linearColor.b()),
            linearColor.a()
        );
    }

    /**
 * @brief 创建符合 glTF 标准的 OSG 材质（使用标准 sRGB 编码）
 * 推荐用于绝大多数 glTF 模型渲染。
 */
    osg::ref_ptr<osg::Material> createOSGMaterialFromPBR(
        const osg::Vec4& baseColorFactor,
        float metallicFactor = 0.0f,
        float roughnessFactor = 1.0f)
    {
        osg::Vec4 baseColorSRGB = linearToSRGB(baseColorFactor);
        osg::ref_ptr<osg::Material> material = new osg::Material;

        if (metallicFactor > 0.5f) {
            material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
            material->setSpecular(osg::Material::FRONT_AND_BACK, baseColorSRGB);
        }
        else {
            material->setDiffuse(osg::Material::FRONT_AND_BACK, baseColorSRGB);
            material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.1f, 0.1f, 0.1f, 1.0f));
        }

        float shininess = (1.0f - clamp(roughnessFactor, 0.0f, 1.0f)) * 128.0f;
        material->setShininess(osg::Material::FRONT_AND_BACK, shininess);
        material->setAmbient(osg::Material::FRONT_AND_BACK, baseColorSRGB * 0.6f);

        return material;
    }

    /**
     * @brief 创建使用自定义 Gamma 的 OSG 材质（非标准，用于调试或特殊需求）
     * 注意：不符合 glTF 官方规范，慎用于生产环境。
     */
    osg::ref_ptr<osg::Material> createOSGMaterialFromPBRWithCustomGamma(
        const osg::Vec4& baseColorFactor,
        float metallicFactor = 0.0f,
        float roughnessFactor = 1.0f,
        float gamma = 2.2f)
    {
        osg::Vec4 baseColorGamma = linearToGammaSpace(baseColorFactor, gamma);
        osg::ref_ptr<osg::Material> material = new osg::Material;

        if (metallicFactor > 0.5f) {
            material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
            material->setSpecular(osg::Material::FRONT_AND_BACK, baseColorGamma);
        }
        else {
            material->setDiffuse(osg::Material::FRONT_AND_BACK, baseColorGamma);
            material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.1f, 0.1f, 0.1f, 1.0f));
        }

        float shininess = (1.0f - clamp(roughnessFactor, 0.0f, 1.0f)) * 128.0f;
        material->setShininess(osg::Material::FRONT_AND_BACK, shininess);
        material->setAmbient(osg::Material::FRONT_AND_BACK, baseColorGamma * 0.2f);

        return material;
    }

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

	NodeBuilder::NodeBuilder(CesiumGltf::Model* model, const glm::dmat4& transform)
		: m_model(model), m_transform(transform)
	{
	}

	osg::Node* NodeBuilder::build() {
        osg::MatrixTransform* root = new osg::MatrixTransform;
        osg::Matrixd matrix;

        glm::dmat4x4 rootTransform = m_transform;
        rootTransform = CesiumGltfContent::GltfUtilities::applyRtcCenter(*m_model, rootTransform);
        rootTransform = CesiumGltfContent::GltfUtilities::applyGltfUpAxisTransform(*m_model, rootTransform);
        matrix.set(glm::value_ptr(rootTransform));

        root->setMatrix(matrix);

        int nodeCount = 0;
        if (!m_model->scenes.empty()) {
            CO_DEBUG("Processing {} scenes", m_model->scenes.size());
            for (size_t sceneIdx = 0; sceneIdx < m_model->scenes.size(); sceneIdx++) {
                auto& scene = m_model->scenes[sceneIdx];
                CO_DEBUG("Scene {} has {} nodes", sceneIdx, scene.nodes.size());
                for (size_t nodeIdx = 0; nodeIdx < scene.nodes.size(); nodeIdx++) {
                    int node = scene.nodes[nodeIdx];
                    if (node >= 0 && node < m_model->nodes.size()) {
                        osg::Node* childNode = createNode(m_model->nodes[node]);
                        if (childNode) {
                            root->addChild(childNode);
                            nodeCount++;
                            CO_TRACE("Added node {} to scene root", nodeIdx);
                        }
                    } else {
                        CO_WARN("Invalid node index {} in scene {}", node, sceneIdx);
                    }
                }
            }
        }
        else {
            CO_DEBUG("No scenes defined, processing all {} nodes directly", m_model->nodes.size());
            for (size_t nodeIdx = 0; nodeIdx < m_model->nodes.size(); nodeIdx++) {
                auto& node = m_model->nodes[nodeIdx];
                // 检查节点是否被其他节点引用作为子节点
                bool isChildNode = false;
                for (const auto& parent : m_model->nodes) {
                    for (int child : parent.children) {
                        if (child == nodeIdx) {
                            isChildNode = true;
                            break;
                        }
                    }
                    if (isChildNode) break;
                }
                
                // 只添加根节点（未被其他节点引用作为子节点的节点）
                if (!isChildNode) {
                    osg::Node* childNode = createNode(node);
                    if (childNode) {
                        root->addChild(childNode);
                        nodeCount++;
                        CO_TRACE("Added root node {} directly", nodeIdx);
                    }
                }
            }
        }

        osg::Group* container = new osg::Group;
        container->addChild(root);
        
        CO_INFO("Built scene with {} nodes", nodeCount);
        
        // 强制更新边界
        container->dirtyBound();
        
        // 添加调试信息
        osg::BoundingSphere bs = container->getBound();
        CO_DEBUG("Final container bounding sphere: center=({},{},{}), radius={}", 
                bs.center().x(), bs.center().y(), bs.center().z(), bs.radius());
        
        // 如果边界仍然无效，尝试手动计算
        if (bs.radius() <= 0) {
            CO_WARN("Container bounding sphere is invalid, computing manually");
            osg::BoundingBox bb;
            for (unsigned int i = 0; i < container->getNumChildren(); ++i) {
                osg::Node* child = container->getChild(i);
                osg::BoundingSphere childBS = child->getBound();
                if (childBS.radius() > 0) {
                    // 使用边界球创建一个包围盒
                    osg::Vec3 center = childBS.center();
                    float radius = childBS.radius();
                    osg::BoundingBox childBB(center - osg::Vec3(radius, radius, radius),
                                            center + osg::Vec3(radius, radius, radius));
                    if (childBB.valid()) {
                        bb.expandBy(childBB);
                    }
                }
            }
            if (bb.valid()) {
                bs.set(bb.center(), bb.radius());
                CO_DEBUG("Manually computed bounding sphere: center=({},{},{}), radius={}", 
                        bs.center().x(), bs.center().y(), bs.center().z(), bs.radius());
            }
        }
        
        return container;
    }

	osg::Node* NodeBuilder::createNode(const CesiumGltf::Node& node) {
		osg::MatrixTransform* root = new osg::MatrixTransform;

		bool matrixSet = false;
		if (node.matrix.size() == 16) {
			osg::Matrixd matrix;
			matrix.set(node.matrix.data());
			root->setMatrix(matrix);
			matrixSet = true;
			CO_TRACE("Node matrix set from node.matrix");
		}

		if (!matrixSet) {
			if (root->getMatrix().isIdentity()) {
				osg::Matrixd scale, translation, rotation;
				if (node.scale.size() == 3) {
					scale = osg::Matrixd::scale(node.scale[0], node.scale[1], node.scale[2]);
					CO_TRACE("Node scale: ({}, {}, {})", node.scale[0], node.scale[1], node.scale[2]);
				} else {
					scale.makeIdentity(); // 默认单位缩放
				}

				if (node.rotation.size() == 4) {
					osg::Quat quat(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
					rotation.makeRotate(quat);
					CO_TRACE("Node rotation: ({}, {}, {}, {})", node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
				} else {
					rotation.makeIdentity(); // 默认无旋转
				}

				if (node.translation.size() == 3) {
					translation = osg::Matrixd::translate(node.translation[0], node.translation[1], node.translation[2]);
					CO_TRACE("Node translation: ({}, {}, {})", node.translation[0], node.translation[1], node.translation[2]);
				} else {
					translation.makeIdentity(); // 默认无平移
				}

				// root->setMatrix(rotation * scale * translation);
				// 应用变换顺序：平移 * 旋转 * 缩放
				root->setMatrix(translation * rotation * scale);
			}
		}

		 // 处理网格
        if (node.mesh >= 0 && node.mesh < m_model->meshes.size()) {
            osg::Node* meshNode = createMesh(m_model->meshes[node.mesh]);
            if (meshNode) {
                root->addChild(meshNode);
            }
        }

        // 递归处理子节点
        for (int child : node.children) {
            if (child >= 0 && child < m_model->nodes.size()) {
                root->addChild(createNode(m_model->nodes[child]));
            }
        }

		return root;
	}

	osg::Node* NodeBuilder::createMesh(const CesiumGltf::Mesh& mesh) {
        osg::Group* group = new osg::Group;

        CO_TRACE("Creating mesh with {} primitives", mesh.primitives.size());

        for (size_t primIndex = 0; primIndex < mesh.primitives.size(); primIndex++) {
            osg::ref_ptr<osg::Geode> geode = new osg::Geode;
            osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;

            // 添加标志确保几何体可见
            geometry->setUseDisplayList(false);
            geometry->setUseVertexBufferObjects(true);
            
            // 处理顶点属性
            int vertexCount = 0;
            osg::Vec3Array* vertices = nullptr;
            const auto& primitive = mesh.primitives[primIndex];

            for (const auto& attribute : primitive.attributes) {
                const std::string& attributeName = attribute.first;
                int accessorIndex = attribute.second;

                if (accessorIndex < 0 || accessorIndex >= m_model->accessors.size()) {
                    CO_WARN("Invalid accessor index {} for attribute {}", accessorIndex, attributeName);
                    continue;
                }

                const auto& accessor = m_model->accessors[accessorIndex];

                if (attributeName == "POSITION") {
                    auto positionView = CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC3<float>>(*m_model, accessor);
                    if (positionView.status() == CesiumGltf::AccessorViewStatus::Valid) {
                        vertices = static_cast<osg::Vec3Array*>(accessorViewToArray(positionView));
                        if (vertices) {
                            geometry->setVertexArray(vertices);
                            vertexCount = vertices->size();
                            CO_TRACE("Position attribute set with {} vertices", vertexCount);
                        }
                    } else {
                        CO_WARN("Invalid position accessor view: {}", static_cast<int>(positionView.status()));
                    }
                }
                else if (attributeName == "NORMAL") {
                    auto normalView = CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC3<float>>(*m_model, accessor);
                    if (normalView.status() == CesiumGltf::AccessorViewStatus::Valid) {
                        osg::Array* normals = accessorViewToArray(normalView);
                        if (normals) {
                            geometry->setNormalArray(normals);
                            geometry->setNormalBinding(osg::Geometry::BIND_PER_VERTEX);
                            CO_TRACE("Normal attribute set");
                        }
                    }
                }
                else if (attributeName == "TEXCOORD_0") {
                    auto texCoordView = CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC2<float>>(*m_model, accessor);
                    if (texCoordView.status() == CesiumGltf::AccessorViewStatus::Valid) {
                        osg::Array* texCoords = accessorViewToArray(texCoordView);
                        if (texCoords) {
                            geometry->setTexCoordArray(0, texCoords);
                            CO_TRACE("Texture coordinate attribute set");
                        }
                    }
                }
            }

            // 处理索引
            bool hasIndices = false;
            if (primitive.indices >= 0 && primitive.indices < m_model->accessors.size()) {
                const auto& indexAccessor = m_model->accessors[primitive.indices];

                if (indexAccessor.componentType == CesiumGltf::Accessor::ComponentType::UNSIGNED_SHORT) {
                    auto indexView = CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::SCALAR<uint16_t>>(*m_model, indexAccessor);
                    if (indexView.status() == CesiumGltf::AccessorViewStatus::Valid) {
                        osg::UShortArray* indices = static_cast<osg::UShortArray*>(accessorViewToArray(indexView));
                        if (indices) {
                            osg::DrawElementsUShort* drawElements = new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES);
                            for (size_t i = 0; i < indices->size(); ++i) {
                                drawElements->push_back((*indices)[i]);
                            }
                            geometry->addPrimitiveSet(drawElements);
                            hasIndices = true;
                            CO_TRACE("Added {} unsigned short indices", indices->size());
                        }
                    }
                }
                else if (indexAccessor.componentType == CesiumGltf::Accessor::ComponentType::UNSIGNED_INT) {
                    auto indexView = CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::SCALAR<uint32_t>>(*m_model, indexAccessor);
                    if (indexView.status() == CesiumGltf::AccessorViewStatus::Valid) {
                        osg::UIntArray* indices = static_cast<osg::UIntArray*>(accessorViewToArray(indexView));
                        if (indices) {
                            osg::DrawElementsUInt* drawElements = new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
                            for (size_t i = 0; i < indices->size(); ++i) {
                                drawElements->push_back((*indices)[i]);
                            }
                            geometry->addPrimitiveSet(drawElements);
                            hasIndices = true;
                            CO_TRACE("Added {} unsigned int indices", indices->size());
                        }
                    }
                }
            }
            
            // 如果没有索引，使用顶点数组
            if (!hasIndices) {
                if (geometry->getVertexArray() && geometry->getVertexArray()->getNumElements() > 0) {
                    int numVertices = geometry->getVertexArray()->getNumElements();
                    geometry->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, numVertices));
                    CO_TRACE("Using vertex array with {} vertices", numVertices);
                } else {
                    CO_WARN("No valid vertex data for primitive");
                    continue; // 跳过这个图元
                }
            }

            // 处理材质和纹理
            //osg::ref_ptr<osg::StateSet> stateSet = geometry->getOrCreateStateSet();
            osg::ref_ptr<osg::StateSet> stateSet = geode->getOrCreateStateSet();
            stateSet->setMode(GL_CULL_FACE, osg::StateAttribute::ON);

            // 确保 geometry 没有自己的 StateSet
            if (geometry->getStateSet()) {
                CO_WARN("Geometry has its own StateSet! This may override Geode's material.");
            }
            
            // 启用光照
            stateSet->setMode(GL_LIGHTING, osg::StateAttribute::ON);
            stateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);

            // 检查primitive是否有材质
            bool hasValidMaterial = false;
			bool hasValidTexture = false;
            if (primitive.material >= 0 && primitive.material < m_model->materials.size()) {
                const auto& material = m_model->materials[primitive.material];
                CO_TRACE("Processing material {}", primitive.material);
                // 处理PBR材质
                if (material.pbrMetallicRoughness) {
                    const auto& pbr = *material.pbrMetallicRoughness;

                    // 获取 PBR 参数
                    auto& baseColorFactor = pbr.baseColorFactor;
                    float metallicFactor = pbr.metallicFactor;
                    float roughnessFactor = pbr.roughnessFactor;

                    //// 基础颜色（直接使用原始 baseColorFactor（线性空间），让 OSG 自动处理 sRGB）
                    //osg::Vec4 baseColor(0.8f, 0.8f, 0.8f, 1.0f);
                    //if (baseColorFactor.size() >= 4) {
                    //    baseColor.set(
                    //        baseColorFactor[0],
                    //        baseColorFactor[1],
                    //        baseColorFactor[2],
                    //        baseColorFactor[3]
                    //    );
                    //    CO_TRACE("Base color: ({}, {}, {}, {})", baseColor.r(), baseColor.g(), baseColor.b(), baseColor.a());
                    //    
                    //    // 不要 gammaCorrect！直接用原始值
                    //    osg::ref_ptr<osg::Material> osgMaterial = new osg::Material;

                    //    if (metallicFactor > 0.5f) {
                    //        osgMaterial->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
                    //        osgMaterial->setSpecular(osg::Material::FRONT_AND_BACK, baseColor); // 原始线性值
                    //    }
                    //    else
                    //    {
                    //        osgMaterial->setDiffuse(osg::Material::FRONT_AND_BACK, baseColor); // 原始线性值
                    //        osgMaterial->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.1f, 0.1f, 0.1f, 1.0f));
                    //    }
                    //    osgMaterial->setShininess(osg::Material::FRONT_AND_BACK, (1.0f - roughnessFactor) * 128.0f);
                    //    osgMaterial->setAmbient(osg::Material::FRONT_AND_BACK, baseColor * 0.6f);

                    //    // 设置材质
                    //    stateSet->setAttributeAndModes(osgMaterial/*, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE */);
                    //    hasValidMaterial = true;
                    //}
                    //else
                    //{
                    //    CO_WARN("Invalid baseColorFactor size: {}", pbr.baseColorFactor.size());
                    //}

                    // 基础颜色（Gamma 校正）
                    osg::Vec4 baseColor(0.8f, 0.8f, 0.8f, 1.0f);
                    if (baseColorFactor.size() >= 4) {
                        baseColor.set(
                            baseColorFactor[0],
                            baseColorFactor[1],
                            baseColorFactor[2],
                            baseColorFactor[3]
                        );

						// 从 PBR 参数创建 OSG 材质（含 Gamma 校正）
                        osg::ref_ptr<osg::Material> osgMaterial = createOSGMaterialFromPBR(
                            baseColor, metallicFactor, roughnessFactor
                        );

                        // 设置材质
                        stateSet->setAttributeAndModes(osgMaterial/*, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE */);
                        hasValidMaterial = true;
                    } else {
                        CO_WARN("Invalid baseColorFactor size: {}", pbr.baseColorFactor.size());
                    }

                    // 处理基础颜色纹理贴图
                    if (pbr.baseColorTexture && pbr.baseColorTexture->index >= 0 &&
                        pbr.baseColorTexture->index < m_model->textures.size())
                    {
                        const auto& texture = m_model->textures[pbr.baseColorTexture->index];
                        CO_TRACE("Loading base color texture {}", pbr.baseColorTexture->index);

                        if (texture.source >= 0 && texture.source < m_model->images.size())
                        {
                            const auto& image = m_model->images[texture.source];

                            // 检查图像资产是否存在
                            if (image.pAsset && !image.pAsset->pixelData.empty())
                            {
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

                                stateSet->setTextureAttributeAndModes(0, osgTexture/*, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE*/);
                                //stateSet->setTextureMode(0, GL_TEXTURE_2D, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
                                hasValidTexture = true;
                                CO_TRACE("Texture applied successfully");
                            }
                        }
                    }
                }
			}
            
            if (!hasValidMaterial && !hasValidTexture) {
                CO_WARN("No valid material found for primitive {}, using default white material");
                // 设置默认材质
                osg::Material* defaultMaterial = new osg::Material;
                defaultMaterial->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.8f, 0.8f, 0.8f, 1.0f));
                defaultMaterial->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.3f, 0.3f, 0.3f, 1.0f));
                defaultMaterial->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.1f, 0.1f, 0.1f, 1.0f));
                defaultMaterial->setShininess(osg::Material::FRONT_AND_BACK, 32.0f);
                stateSet->setAttributeAndModes(defaultMaterial, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
            }

            geode->addDrawable(geometry);
            group->addChild(geode);
        }

        // 添加调试信息
        osg::BoundingSphere bs = group->getBound();
        CO_DEBUG("Created mesh node with bounding sphere: center=({},{},{}), radius={}", 
                bs.center().x(), bs.center().y(), bs.center().z(), bs.radius()
        );
        
        return group;
    }

}	// namespace czmosg