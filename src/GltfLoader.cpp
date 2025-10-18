#include "GltfLoader.h"
#include "NodeBuilder.h"
//#include "AsyncSystemWrapper.h"
#include "SimpleAssetAccessor.h"
#include "AsyncTaskProcessor.h"
#include "RuntimeSupport.h"
#include "Log.h"

#include <osg/MatrixTransform>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumAsync/IAssetResponse.h>
#include <CesiumGltfContent/GltfUtilities.h>

#include <filesystem>

namespace
{
    // Wrapper class that allows shutting down the AsyncSystem when the program exits.
    class AsyncSystemWrapper
    {
    public:
        AsyncSystemWrapper()
            : taskProcessor(std::make_shared<AsyncTaskProcessor>()),
            asyncSystem(taskProcessor)
        {
        }
        CesiumAsync::AsyncSystem& getAsyncSystem() noexcept;
        void shutdown();
        std::shared_ptr<AsyncTaskProcessor> taskProcessor;
        CesiumAsync::AsyncSystem asyncSystem;
    };

    AsyncSystemWrapper& getAsyncSystemWrapper()
    {
        static AsyncSystemWrapper wrapper;
        return wrapper;
    }

    CesiumAsync::AsyncSystem& getAsyncSystem() noexcept
    {
        return getAsyncSystemWrapper().asyncSystem;
    }
}

namespace czmosg
{
    namespace fs = std::filesystem;

    struct ParseGltfResult
    {
        ParseGltfResult(const std::string& error, const std::shared_ptr<CesiumAsync::IAssetRequest>& in_request)
            : request(in_request)
        {
            gltfResult.errors.push_back(error);
        }
        ParseGltfResult(CesiumGltfReader::GltfReaderResult&& result,
            const std::shared_ptr<CesiumAsync::IAssetRequest>& in_request)
            : gltfResult(std::move(result)) ,request(in_request)
        {
        }

        ParseGltfResult() = default;
        CesiumGltfReader::GltfReaderResult gltfResult;
        std::shared_ptr<CesiumAsync::IAssetRequest> request;
    };

    struct ReadGltfResult
    {
        osg::ref_ptr<osg::Node> node;
        std::vector<std::string> errors;
    };

    GltfLoader::GltfLoader()
    {
        m_assetAccessor = std::make_shared<SimpleAssetAccessor>();
	}

    // CesiumAsync::Future<GltfLoader::ReadGltfResult> GltfLoader::loadGltfNode(const std::string& uri) const
    // {
    //     std::vector<CesiumAsync::IAssetAccessor::THeader> headers;
    //     return reader.loadGltf(getAsyncSystem(), uri, headers, m_assetAccessor, readerOptions)
    //         .thenInWorkerThread([this](CesiumGltfReader::GltfReaderResult&& gltfResult)
    //         {
    //             // 检查是否有错误
    //             if (!gltfResult.errors.empty()) {
    //                 for (const auto& error : gltfResult.errors) {
    //                     CO_ERROR("GLTF loading error: {}", error);
    //                 }
    //             }

    //             if (!gltfResult.warnings.empty()) {
    //                 for (const auto& warning : gltfResult.warnings) {
    //                     CO_WARN("GLTF loading warning: {}", warning);
    //                 }
    //             }

    //             // 检查模型是否有效
    //             if (!gltfResult.model) {
    //                 CO_ERROR("GLTF model is null");
    //                 return ReadGltfResult{nullptr, {"GLTF model is null"}};
    //             }

    //             //CreateModelOptions modelOptions{};
    //             NodeBuilder nodeBuilder(&*gltfResult.model, glm::dmat4());
    //             glm::dmat4 yUp(1.0);
    //             yUp = CesiumGltfContent::GltfUtilities::applyGltfUpAxisTransform(*gltfResult.model, yUp);
    //             auto modelNode = nodeBuilder.build();

    //             if (!modelNode) {
    //                 CO_ERROR("Failed to build OSG node from GLTF model");
    //                 return ReadGltfResult{nullptr, {"Failed to build OSG node"}};
    //             }

    //             // 检查构建的节点
    //             osg::BoundingSphere bs = modelNode->getBound();
    //             CO_DEBUG("Built node bounding sphere: center=({},{},{}), radius={}", 
    //                 bs.center().x(), bs.center().y(), bs.center().z(), bs.radius()
    //             );

    //             if (isIdentity(yUp))
    //             {
    //                 return ReadGltfResult{modelNode, {}};
    //             }
                
	// 			auto transformNode = new osg::MatrixTransform(glm2osg(yUp));
    //             transformNode->addChild(modelNode);
    //             return ReadGltfResult{transformNode, {}};
    //         });
    // }

    osg::ref_ptr<osg::Node> GltfLoader::read(const std::string& filePath) const
    {
        using namespace CesiumGltfReader;
        std::string uriPath;
        if (filePath.starts_with("http:") || filePath.starts_with("https:"))
        {
            uriPath = filePath;
        }
        else
        {
            if (!fs::exists(filePath))
            {
                CO_CRITICAL("Can't find file {} ", filePath);
            }
            // Really need an absolute path in order to make a URI
            auto absPath = std::filesystem::absolute(filePath);
            uriPath = "file://" + absPath.string();
        }
        auto future = loadGltfNode(uriPath);
        if (isMainThread())
        {
            // Can't block the dispatch of main thread tasks
            while (!future.isReady())
            {
                getAsyncSystem().dispatchMainThreadTasks();
            }
        }

        auto loadResult = future.wait();

        return loadResult.node;
    }

    CesiumAsync::Future<GltfLoader::ReadGltfResult> GltfLoader::loadGltfNode(const std::string& uri) const
    {
        std::vector<CesiumAsync::IAssetAccessor::THeader> headers;
        return reader.loadGltf(getAsyncSystem(), uri, headers, m_assetAccessor, readerOptions)
            .thenInWorkerThread([this](CesiumGltfReader::GltfReaderResult&& gltfResult)
            {
                // 检查是否有错误
                if (!gltfResult.errors.empty()) {
                    for (const auto& error : gltfResult.errors) {
                        CO_WARN("GLTF loading error: {}", error);
                    }
                }
                
                if (!gltfResult.warnings.empty()) {
                    for (const auto& warning : gltfResult.warnings) {
                        CO_WARN("GLTF loading warning: {}", warning);
                    }
                }
                
                // 检查模型是否有效
                if (!gltfResult.model) {
                    CO_ERROR("GLTF model is null");
                    return ReadGltfResult{nullptr, {"GLTF model is null"}};
                }
                
                CO_INFO("GLTF model loaded successfully with {} scenes, {} nodes, {} meshes", 
                       gltfResult.model->scenes.size(), 
                       gltfResult.model->nodes.size(), 
                       gltfResult.model->meshes.size());
                
                // 创建节点构建器，使用单位矩阵而不是glm::dmat4()
                NodeBuilder nodeBuilder(&*gltfResult.model, glm::dmat4(1.0));
                auto modelNode = nodeBuilder.build();
                
                if (!modelNode) {
                    CO_ERROR("Failed to build OSG node from GLTF model");
                    return ReadGltfResult{nullptr, {"Failed to build OSG node"}};
                }
                
                // 强制更新边界
                modelNode->dirtyBound();
                
                // 检查构建的节点
                osg::BoundingSphere bs = modelNode->getBound();
                CO_DEBUG("Built node bounding sphere: center=({},{},{}), radius={}", 
                        bs.center().x(), bs.center().y(), bs.center().z(), bs.radius());
                
                // 应用Y-up变换（如果需要）
                //glm::dmat4 yUp(1.0);
                //yUp = CesiumGltfContent::GltfUtilities::applyGltfUpAxisTransform(*gltfResult.model, yUp);
                //if (!isIdentity(yUp)) {
                //    auto transformNode = new osg::MatrixTransform(glm2osg(yUp));
                //    transformNode->addChild(modelNode);
                //    
                //    // 更新变换节点的边界
                //    transformNode->dirtyBound();
                //    return ReadGltfResult{transformNode, {}};
                //}
                
                return ReadGltfResult{modelNode, {}};
            });
    }
    
}   // namespace czmosg