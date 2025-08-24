#pragma once

#include <osg/Group>

#include <string>
#include <vector>

// 前向声明
namespace Cesium3DTilesSelection {
    class Tileset;
}

namespace CesiumAsync {
    class AsyncSystem;
}

namespace CesiumUtility {
    class JsonValue;
    class CreditSystem;
}

class AsyncTaskProcessor;
class SimpleAssetAccessor;
class SimpleRenderResourcesPreparer;

class Cesium3DTileset : public osg::Group {
public:
    // 构造函数：支持 URL 和 Asset ID 两种方式
    Cesium3DTileset(const std::string& url, float maximumScreenSpaceError = 16.0f);
    Cesium3DTileset(unsigned int assetID, const std::string& server = "", const std::string& token = "", float maximumScreenSpaceError = 16.0f);
    virtual ~Cesium3DTileset();
    
    // 重写 traverse 方法来处理瓦片集更新和渲染
    virtual void traverse(osg::NodeVisitor& nv) override;
    
    // 重写 computeBound 方法
    virtual osg::BoundingSphere computeBound() const override;
    
    // 设置和获取最大屏幕空间误差
    void setMaximumScreenSpaceError(float error);
    float getMaximumScreenSpaceError() const;
    
    // 设置和获取是否禁止孔洞
    void setForbidHoles(bool forbidHoles);
    bool getForbidHoles() const;
    
    // 检查根瓦片是否可用
    bool isRootTileAvailable() const;
    
private:
    void* m_tileset; // 指向 Cesium3DTilesSelection::Tileset 的指针
    
    std::shared_ptr<AsyncTaskProcessor> m_taskProcessor ;
    std::shared_ptr<CesiumAsync::AsyncSystem> m_asyncSystem;
    std::shared_ptr<SimpleAssetAccessor> m_assetAccessor;
    std::shared_ptr<SimpleRenderResourcesPreparer> m_prepareRenderResources;
    std::shared_ptr<CesiumUtility::CreditSystem> m_creditSystem;

    // 初始化方法
    void initializeTileset(const std::string& url, float maximumScreenSpaceError);
    void initializeTileset(unsigned int assetID, const std::string& server, const std::string& token, float maximumScreenSpaceError);
};
