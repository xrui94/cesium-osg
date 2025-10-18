#include "Log.h"
#include "Cesium3DTileset.h"
#include "GltfLoader.h"

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

/**
 * @brief 将 osg::Node 保存为 .osg 或 .osgb 文件
 *
 * @param node 要保存的场景节点（不能为 nullptr）
 * @param filename 输出文件路径，扩展名决定格式：
 *                 - ".osg"  → 文本格式（可读）
 *                 - ".osgb" → 二进制格式（更小、更快）
 * @return true 保存成功；false 失败（如文件无法写入、格式不支持等）
 */
static bool saveNodeToFile(osg::Node* node, const std::string& filename)
{
    if (!node)
    {
        std::cerr << "Error: Cannot save null osg::Node to file." << std::endl;
        return false;
    }

    if (filename.empty())
    {
        std::cerr << "Error: Output filename is empty." << std::endl;
        return false;
    }

    // 可选：自动补全扩展名（如果未指定）
    // 例如：若用户传 "model"，可默认保存为 "model.osgb"
    // 此处保持原样，由调用者指定扩展名

    bool success = osgDB::writeNodeFile(*node, filename);
    if (!success)
    {
        std::cerr << "Error: Failed to write node to file: " << filename << std::endl;
        return false;
    }

    std::cout << "Successfully saved node to: " << filename << std::endl;
    return true;
}

static void zoomToModel(osgViewer::Viewer* viewer, osg::Node* model)
{
    if (!model || !viewer) return;

    // 1. 获取模型的包围球（在世界坐标系下）
    osg::BoundingSphere bs = model->getBound();
    if (bs.radius() == 0.0) return; // 空模型

    // 2. 获取当前相机操纵器（假设是 TrackballManipulator）
    osgGA::TrackballManipulator* manipulator =
        dynamic_cast<osgGA::TrackballManipulator*>(viewer->getCameraManipulator());

    if (!manipulator)
    {
        // 如果没有，创建一个
        manipulator = new osgGA::TrackballManipulator;
        viewer->setCameraManipulator(manipulator);
    }

    // 3. 设置操纵器的目标中心和距离
    const osg::Vec3& center = bs.center();
    double radius = bs.radius();

    // 计算合适的相机距离：
    // 使用视场角（FOV）和包围球半径来保证模型完整可见
    double fov = osg::DegreesToRadians(30.0); // 默认垂直视场角（可从相机获取）
    if (viewer->getCamera()/* && viewer->getCamera()->getProjectionMatrix()*/)
    {
        // 更精确：从当前相机投影矩阵提取 FOV
        double fovy, aspectRatio, zNear, zFar;
        viewer->getCamera()->getProjectionMatrix().getPerspective(fovy, aspectRatio, zNear, zFar);
        fov = osg::DegreesToRadians(fovy);
    }

    // 距离 = 半径 / sin(fov/2)  —— 保证包围球刚好填满视口垂直方向
    double distance = radius / sin(fov * 0.5);

    // 设置操纵器状态
    manipulator->setCenter(center);
    manipulator->setDistance(distance);

    // 4. 立即应用（可选）
    manipulator->home(0.0); // 0.0 表示不带动画，立即跳转
}

int main(int argc, char** argv)
{
    // 需要显式创建并持有，以控制生命周期
    // 初始化日志系统
    czmosg::initializeLogger();

    // 创建根节点
    osg::ref_ptr<osg::Group> root = new osg::Group();

    // 创建 Viewer 并设置场景
    osgViewer::Viewer viewer;
    viewer.apply(new osgViewer::SingleWindow(200, 100, 1280, 720));
    viewer.setSceneData(root);

    // 创建光源
    osg::ref_ptr<osg::Light> light = new osg::Light;
    light->setLightNum(0);
    light->setPosition(osg::Vec4(0.0f, 0.0f, 1.0f, 0.0f)); // 方向光（来自上方）
    light->setAmbient(osg::Vec4(0.4f, 0.4f, 0.4f, 1.0f));   // 环境光避免全黑
    light->setDiffuse(osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f));   // 主光

    // 创建 LightSource 节点
    osg::ref_ptr<osg::LightSource> lightSource = new osg::LightSource;
	lightSource->setLight(light);
    root->addChild(lightSource); // rootNode 是你加载的 glTF 场景根节点


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

    //
    czmosg::GltfLoader gltfLoader;
    //const std::string gltfUrl = "D:/xrui94/data/models/glTF-Sample-Models-main/2.0/Cube/glTF/Cube.gltf";
    //const std::string gltfUrl = "D:\\xrui94\\projs\\projx_xrui94_learn_threejs\\three.js-r170\\manual\\examples\\resources\\models\\mountain_landscape\\scene.gltf";
	const std::string gltfUrl = "http://127.0.0.1:9095/models/Bee/Bee.gltf";
    osg::ref_ptr<osg::Node> gltfModel = gltfLoader.read(gltfUrl);

    // 添加调试信息
    if (gltfModel.valid()) {
        osg::BoundingSphere bs = gltfModel->getBound();
        CO_INFO("Model bounding sphere: center=({},{},{}), radius={}", 
                bs.center().x(), bs.center().y(), bs.center().z(), bs.radius());
        
        // 检查模型是否为空
        if (bs.radius() <= 0) {
            CO_WARN("Model appears to be empty or invalid!");
            // 尝试强制计算边界
            const_cast<osg::Node*>(gltfModel.get())->dirtyBound();
            bs = gltfModel->getBound();
            CO_INFO("After dirtyBound(): center=({},{},{}), radius={}", 
                    bs.center().x(), bs.center().y(), bs.center().z(), bs.radius());
        }
        
        // 检查节点类型
        CO_INFO("Model node type: {}", gltfModel->className());
        
        // 检查子节点数量
        osg::Group* group = gltfModel->asGroup();
        if (group) {
            CO_INFO("Model has {} children", group->getNumChildren());
        }

        saveNodeToFile(gltfModel, "D:/scene_czmosg.osg");
        lightSource->addChild(gltfModel);
        //zoomToModel(&viewer, gltfModel);
        // 对于普通OSG模型，使用标准渲染循环
        return viewer.run();
    }
    else {
        CO_ERROR("Failed to load GLTF model from: {}", gltfUrl);
    }


    //const std::string tilesetUrl = "http://127.0.0.1:9095/models/DaYanTa_3DTiles1.0/tileset.json";    // 支持“http://”和“https://”协议
    ////const std::string tilesetUrl = "D:/xrui94/data/models/DaYanTa_3DTiles1.0/tileset.json"; // 支持本地模型加载
    ////const std::string tilesetUrl = "file://D:/xrui94/data/models/DaYanTa_3DTiles1.0/tileset.json"; // 支持“file://”
    ////const std::string tilesetUrl = "file:///D:/xrui94/data/models/DaYanTa_3DTiles1.0/tileset.json"; // 支持“file:///”（更推荐这种规范的 file 协议）

    //osg::ref_ptr<osg::Node> model = new Cesium3DTileset(tilesetUrl, 16.0f); // 降低maximumScreenSpaceError

    //// 检查模型是否加载成功
    //if (!model) {
    //    CO_CRITICAL("Error: Could not load model file: {}", tilesetUrl);
    //    return 1;
    //}
    //CO_INFO("Cesium 3DTiles Model loaded successfully: {}", tilesetUrl);

    //// 将模型添加到场景图
    //root->addChild(model);



    //// 设置视点位置，不能和漫游器同时使用，否则就会不起作用
    ////osg::BoundingSphere bs = model->computeBound();
    ////osg::Vec3d eye = osg::Vec3(bs.center().x(), -bs.center().y() * 10, bs.center().z());
    ////osg::Vec3d center = bs.center();
    ////osg::Vec3d up = osg::Vec3(0.0, 0.0, 1.0);

    //// 参考osgEarth的实现，使用TrackballManipulator来支持交互
    //viewer.setCameraManipulator(new osgGA::TrackballManipulator());
    //CO_TRACE("Using TrackballManipulator for interactive camera handling");

    //
    //// 添加统计信息处理器
    //viewer.addEventHandler(new osgViewer::StatsHandler);
    //
    //// 检查是否是Cesium瓦片集
    //Cesium3DTileset* cesiumNode = dynamic_cast<Cesium3DTileset*>(model.get());
    //if (cesiumNode) {
    //    // 对于Cesium瓦片集，使用自定义渲染循环
    //    // 注意：不需要手动调用dispatchMainThreadTasks，traverse方法中已经处理
    //    // 主循环 - 运行更长时间以允许异步加载完成
    //    int frameCount = 0;
    //    bool homePositioned = false;
    //    //const int maxFrames = 1000; // 运行300帧
    //    
    //    while (!viewer.done()/* && frameCount < maxFrames*/)
    //    {
    //        // 注意不要使用viewer->run()，如果使用这个参数，上面关于相机的所有更改都会无效
    //        viewer.frame();
    //        frameCount++;
    //        
    //        // 检查瓦片集的根瓦片是否已经可用，如果可用且还没有调用过home()，则立即调用
    //        if (!homePositioned && frameCount > 10) { // 等待至少10帧以确保初始化完成
    //            // 使用新的isRootTileAvailable方法检查根瓦片状态
    //            if (cesiumNode->isRootTileAvailable()) {
    //                CO_DEBUG("Root tile available, setting manual camera position to model area");
    //                
    //                // 检查相机操作器是否有效
    //                osgGA::CameraManipulator* manipulator = viewer.getCameraManipulator();
    //                if (manipulator) {
    //                    // 大雁塔附近的坐标 (西安)
    //                    osg::Vec3d eye(-1.71545e+06 + 100.0, 4.99352e+06 + 100.0, 3.56687e+06 + 500.0);
    //                    osg::Vec3d center(-1.71545e+06, 4.99352e+06, 3.56687e+06);
    //                    osg::Vec3d up(0.0, 0.0, 1.0);
    //                    
    //                    CO_TRACE("Setting home position...");
    //                    manipulator->setHomePosition(eye, center, up);
    //                    CO_TRACE("Calling viewer.home()...");
    //                    viewer.home();
    //                    homePositioned = true;
    //                    CO_TRACE("Camera positioned at: ({}, {}, {})", eye.x(), eye.y(), eye.z());
    //                } else {
    //                    CO_ERROR("Error: Camera manipulator is null!");
    //                }
    //            }
    //        }
    //        
    //        // 每10帧添加一个小延迟，给异步任务更多时间
    //        if (frameCount % 10 == 0) {
    //            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    //        }
    //    }
    //    
    //    CO_TRACE("Completed {} frames", frameCount);
    //    //bool success = osgDB::writeNodeFile(*root, "filename.osg");
    //    //if (!success) std::cerr << "Failed to save to osg file." << std::endl;
    //    return 0;
    //} else {
    //    // 对于普通OSG模型，使用标准渲染循环
    //    return viewer.run();
    //}
}