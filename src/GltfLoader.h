#pragma once

#include <CesiumAsync/Future.h>
#include <CesiumGltfReader/GltfReader.h>

#include <osg/Node>

class SimpleAssetAccessor;

namespace czmosg
{

    class GltfLoader
    {
    public:
        GltfLoader();
        osg::ref_ptr<osg::Node> read(const std::string& filePath) const;

    protected:
        struct ReadGltfResult
        {
            osg::ref_ptr<osg::Node> node;
            std::vector<std::string> errors;
        };

        CesiumAsync::Future<ReadGltfResult> loadGltfNode(const std::string& uri) const;
        CesiumGltfReader::GltfReader reader;
        CesiumGltfReader::GltfReaderOptions readerOptions;

    private:
        std::shared_ptr<SimpleAssetAccessor> m_assetAccessor;
    };

}   // namespace czmosg