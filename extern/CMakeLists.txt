
# cesium-native
# From here: https://github.com/CesiumGS/cesium-native (Tag: v0.49.0)

# Cesium native wrapper 
add_subdirectory(cesium-native EXCLUDE_FROM_ALL )

add_library(cesium-native-wrapper INTERFACE)

target_link_libraries(cesium-native-wrapper INTERFACE
    CesiumUtility
    CesiumGltf
    CesiumGeometry
    CesiumGeospatial
    CesiumJsonReader
    CesiumJsonWriter
    CesiumGltfContent
    CesiumGltfReader
    CesiumGltfWriter
    CesiumAsync
    Cesium3DTiles
    Cesium3DTilesReader
    Cesium3DTilesWriter
    Cesium3DTilesContent
    CesiumRasterOverlays
    Cesium3DTilesSelection
    # CesiumIonClient
    CesiumQuantizedMeshTerrain
)

# ${PROJECT_NAME} 会接收从父级目录传进来的 PROJECT_NAME
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        cesium-native-wrapper
)