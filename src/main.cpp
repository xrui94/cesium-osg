#include "Log.h"
#include "Cesium3DTileset.h"

#include <osg/Node>
#include <osg/Group>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include <osgViewer/config/SingleWindow>
#include <osgGA/TrackballManipulator>

#include <string>
#include <thread>
#include <chrono>
#include <algorithm>

//// 判断文件名是否以指定后缀（不区分大小写）结尾
//static bool hasFileExtension(const std::string& filename, const std::string& ext)
//{
//    // ext 应该包含点，例如 ".osgb"
//    if (ext.empty() || ext[0] != '.') {
//        return false;
//    }
//
//    if (filename.length() < ext.length()) {
//        return false;
//    }
//
//    // 提取文件名末尾相同长度的子串
//    std::string fileExt = filename.substr(filename.length() - ext.length());
//
//    // 转换为小写进行比较（不区分大小写）
//    std::transform(fileExt.begin(), fileExt.end(), fileExt.begin(), ::tolower);
//    std::string lowerExt = ext;
//    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
//
//    return fileExt == lowerExt;
//}

int main(int argc, char** argv)
{
    // 需要显式创建并持有，以控制生命周期
    // 初始化日志系统
    czmosg::initializeLogger();

    // 创建根节点
    osg::ref_ptr<osg::Group> root = new osg::Group();

    // 检查是否传入了文件路径
    //if (argc < 2) {
    //    std::cerr << "Usage: " << argv[0] << " <model.osgb, tileset.json>" << std::endl;
    //    return 1;
    //}

    //const std::string modelPath = argv[1];
    //osg::ref_ptr<osg::Node> model = nullptr;
    //if (hasFileExtension(modelPath, ".osgb"))
    //{
    //    // 使用 osgDB::readNodeFile 加载 .osgb 模型
    //    model = osgDB::readNodeFile(modelPath);
    //}
    //else if (hasFileExtension(modelPath, ".json"))
    //{
    //    model = new Cesium3DTileset(modelPath);
    //}


    const std::string tilesetUrl = "http://127.0.0.1:9095/models/DaYanTa_3DTiles1.0/tileset.json";    // 支持“http://”和“https://”协议
    //const std::string tilesetUrl = "D:/xrui94/data/models/DaYanTa_3DTiles1.0/tileset.json"; // 支持本地模型加载
    //const std::string tilesetUrl = "file://D:/xrui94/data/models/DaYanTa_3DTiles1.0/tileset.json"; // 支持“file://”
    //const std::string tilesetUrl = "file:///D:/xrui94/data/models/DaYanTa_3DTiles1.0/tileset.json"; // 支持“file:///”（更推荐这种规范的 file 协议）

    osg::ref_ptr<osg::Node> model = new Cesium3DTileset(tilesetUrl, 16.0f); // 降低maximumScreenSpaceError

    // 检查模型是否加载成功
    if (!model) {
        CO_CRITICAL("Error: Could not load model file: {}", tilesetUrl);
        return 1;
    }
    CO_INFO("Cesium 3DTiles Model loaded successfully: {}", tilesetUrl);

    // 将模型添加到场景图
    root->addChild(model);

    // 创建 Viewer 并设置场景
    osgViewer::Viewer viewer;
    viewer.apply(new osgViewer::SingleWindow(200, 100, 1280, 720));

    // 设置视点位置，不能和漫游器同时使用，否则就会不起作用
    //osg::BoundingSphere bs = model->computeBound();
    //osg::Vec3d eye = osg::Vec3(bs.center().x(), -bs.center().y() * 10, bs.center().z());
    //osg::Vec3d center = bs.center();
    //osg::Vec3d up = osg::Vec3(0.0, 0.0, 1.0);

    // 参考osgEarth的实现，使用TrackballManipulator来支持交互
    viewer.setCameraManipulator(new osgGA::TrackballManipulator());
    CO_TRACE("Using TrackballManipulator for interactive camera handling");

    viewer.setSceneData(root);
    
    // 添加统计信息处理器
    viewer.addEventHandler(new osgViewer::StatsHandler);
    
    // 检查是否是Cesium瓦片集
    Cesium3DTileset* cesiumNode = dynamic_cast<Cesium3DTileset*>(model.get());
    if (cesiumNode) {
        // 对于Cesium瓦片集，使用自定义渲染循环
        // 注意：不需要手动调用dispatchMainThreadTasks，traverse方法中已经处理
        // 主循环 - 运行更长时间以允许异步加载完成
        int frameCount = 0;
        bool homePositioned = false;
        //const int maxFrames = 1000; // 运行300帧
        
        while (!viewer.done()/* && frameCount < maxFrames*/)
        {
            // 注意不要使用viewer->run()，如果使用这个参数，上面关于相机的所有更改都会无效
            viewer.frame();
            frameCount++;
            
            // 检查瓦片集的根瓦片是否已经可用，如果可用且还没有调用过home()，则立即调用
            if (!homePositioned && frameCount > 10) { // 等待至少10帧以确保初始化完成
                // 使用新的isRootTileAvailable方法检查根瓦片状态
                if (cesiumNode->isRootTileAvailable()) {
                    CO_DEBUG("Root tile available, setting manual camera position to model area");
                    
                    // 检查相机操作器是否有效
                    osgGA::CameraManipulator* manipulator = viewer.getCameraManipulator();
                    if (manipulator) {
                        // 大雁塔附近的坐标 (西安)
                        osg::Vec3d eye(-1.71545e+06 + 100.0, 4.99352e+06 + 100.0, 3.56687e+06 + 500.0);
                        osg::Vec3d center(-1.71545e+06, 4.99352e+06, 3.56687e+06);
                        osg::Vec3d up(0.0, 0.0, 1.0);
                        
                        CO_TRACE("Setting home position...");
                        manipulator->setHomePosition(eye, center, up);
                        CO_TRACE("Calling viewer.home()...");
                        viewer.home();
                        homePositioned = true;
                        CO_TRACE("Camera positioned at: ({}, {}, {})", eye.x(), eye.y(), eye.z());
                    } else {
                        CO_ERROR("Error: Camera manipulator is null!");
                    }
                }
            }
            
            // 每10帧添加一个小延迟，给异步任务更多时间
            if (frameCount % 10 == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        
        CO_TRACE("Completed {} frames", frameCount);
        //bool success = osgDB::writeNodeFile(*root, "filename.osg");
        //if (!success) std::cerr << "Failed to save to osg file." << std::endl;
        return 0;
    } else {
        // 对于普通OSG模型，使用标准渲染循环
        return viewer.run();
    }
}