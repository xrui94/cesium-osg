cmake_minimum_required( VERSION 3.22 )

# Main project information
project(CesiumOsg
    LANGUAGES
        CXX
    VERSION
        0.0.1
)

message(STATUS "Using CMake ${CMAKE_VERSION}")

# Require out-of-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)

if ( EXISTS "${LOC_PATH}" )
    message( FATAL_ERROR "You cannot build in the source directory. Please use a build subdirectory." )
endif()

# Turn on link time optimization for everything
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# Output compile commands to compile_commands.json (for debugging CMake issues)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build universal lib on macOS
# Note that CMAKE_OSX_ARCHITECTURES must be set before project().
if (APPLE)
    set( CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" )
endif()

# Source files
set(CESIUM_OSG_HEADER_FILES
	src/Log.h
	src/RuntimeSupport.h
	src/AsyncSystemWrapper.h
    src/AsyncTaskProcessor.h
    src/SimpleAssetAccessor.h
	src/SimpleRenderResourcesPreparer.h
	src/NodeBuilder.h
	src/GltfLoader.h
    src/Cesium3DTileset.h
)

set(CESIUM_OSG_SOURCE_FILES
	src/Log.cpp
	src/AsyncSystemWrapper.cpp
    src/AsyncTaskProcessor.cpp
    src/SimpleAssetAccessor.cpp
    src/SimpleRenderResourcesPreparer.cpp
	src/NodeBuilder.cpp
	src/GltfLoader.cpp
	src/Cesium3DTileset.cpp
    src/main.cpp
)

# Create our library
add_executable(${PROJECT_NAME} ${CESIUM_OSG_HEADER_FILES} ${CESIUM_OSG_SOURCE_FILES})

target_compile_features(${PROJECT_NAME}
    PRIVATE
        cxx_std_20
)

# 多配置生成器下，目标目录需包含 $<CONFIG>
set(TARGET_DIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>")

#
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4 /WX 
        /wd4100 /wd4244 /wd4267 /wd4996 /wd4828 /wd4010
		/wd4505  # 👈 忽略“未引用的本地函数”警告
        /utf-8
        # # 确保Debug和Release使用正确的运行时库
        # $<$<CONFIG:Debug>:/MDd>
        # $<$<CONFIG:Release>:/MD>
    )
    # # Debug模式保持断言以便调试，Release模式禁用断言
    # target_compile_definitions(${PROJECT_NAME} PRIVATE
    #     $<$<CONFIG:Release>:NDEBUG>
    # )
endif()

# 定义通用的跨平台、跨编译器的“构建类型宏定义”
# 通过“生成器表达式”支持 vs、xcode这种多配置生成器，
# 也适用于 Makefile、Ninja 这种单配置生成器
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
	$<$<CONFIG:Debug>:CO_DEBUG_MODE>
	$<$<CONFIG:Release>:CO_RELEASE_MODE>
	$<$<CONFIG:MinSizeRel>:CO_MINSIZEREL_MODE>
	$<$<CONFIG:RelWithDebInfo>:CO_RELWITHDEBINFO_MODE>
)

# 此设置会导致编译错误，即：库编译配置冲突，尤其是在使用 Cesium Native 和其依赖的 S2 Geometry 库 
# 类似：error LNK2038: 检测到“annotate_string”的不匹配项: 值“0”不匹配值“1”
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../Engine
        ${CMAKE_CURRENT_SOURCE_DIR}/../CesiumNativeGL
)

# 自定义变量，设置三方库的路径
set(THIRD_PARTY_DIR C:/env/libc++)

# OSG
target_include_directories(${PROJECT_NAME} PRIVATE ${THIRD_PARTY_DIR}/osg/Release/include)
# 查找 Debug 版本的库
find_library(LIB_Osgd osgd ${THIRD_PARTY_DIR}/osg/Debug/lib)
find_library(LIB_OsgDBd osgDBd ${THIRD_PARTY_DIR}/osg/Debug/lib)
find_library(LIB_OsgTextd osgTextd ${THIRD_PARTY_DIR}/osg/Debug/lib)
find_library(LIB_OpenThreadsd OpenThreadsd ${THIRD_PARTY_DIR}/osg/Debug/lib)
find_library(LIB_OsgUtild osgUtild ${THIRD_PARTY_DIR}/osg/Debug/lib)
find_library(LIB_OsgViewerd osgViewerd ${THIRD_PARTY_DIR}/osg/Debug/lib)
find_library(LIB_OsgGAd osgGAd ${THIRD_PARTY_DIR}/osg/Debug/lib)
# 查找 Release 版本的库
find_library(LIB_Osg osg ${THIRD_PARTY_DIR}/osg/Release/lib)
find_library(LIB_OsgDB osgDB ${THIRD_PARTY_DIR}/osg/Release/lib)
find_library(LIB_OsgText osgText ${THIRD_PARTY_DIR}/osg/Release/lib)
find_library(LIB_OpenThreads OpenThreads ${THIRD_PARTY_DIR}/osg/Release/lib)
find_library(LIB_OsgUtil osgUtil ${THIRD_PARTY_DIR}/osg/Release/lib)
find_library(LIB_OsgViewer osgViewer ${THIRD_PARTY_DIR}/osg/Release/lib)
find_library(LIB_OsgGA osgGA ${THIRD_PARTY_DIR}/osg/Release/lib)

# 链接 OSG 库
target_link_libraries(
	${PROJECT_NAME} PRIVATE
	debug ${LIB_Osgd} optimized ${LIB_Osg}
	debug ${LIB_OsgDBd} optimized ${LIB_OsgDB}
	debug ${LIB_OsgTextd} optimized ${LIB_OsgText}
	debug ${LIB_OpenThreadsd} optimized ${LIB_OpenThreads}
	debug ${LIB_OsgUtild} optimized ${LIB_OsgUtil}
	debug ${LIB_OsgViewerd} optimized ${LIB_OsgViewer}
	debug ${LIB_OsgGAd} optimized ${LIB_OsgGA}
)

# CURL
target_include_directories(${PROJECT_NAME} PRIVATE ${THIRD_PARTY_DIR}/curl/include)
# 查找 Debug 版本的库
find_library(LIB_CURLd libcurld ${THIRD_PARTY_DIR}/curl/lib/Debug)
# 查找 Release 版本的库
find_library(LIB_CURL libcurl ${THIRD_PARTY_DIR}/curl/lib/Release)

# 链接 CURL 库
target_link_libraries(
	${PROJECT_NAME} PRIVATE
	debug ${LIB_CURLd} optimized ${LIB_CURL}
)

# Windows平台才需要拷贝动态库
# 拷贝OSG动态库到可执行程序所在的目录
# IN LISTS 之后应该是变量名而不是变量值，所以不能加 ${}
if (CMAKE_HOST_WIN32)
	set(LIBS_DEBUG
		# osg	
		${THIRD_PARTY_DIR}/osg/Debug/bin/osgd.dll
		${THIRD_PARTY_DIR}/osg/Debug/bin/osgDBd.dll
		${THIRD_PARTY_DIR}/osg/Debug/bin/osgUtild.dll
		${THIRD_PARTY_DIR}/osg/Debug/bin/osgViewerd.dll
		${THIRD_PARTY_DIR}/osg/Debug/bin/osgTextd.dll
		${THIRD_PARTY_DIR}/osg/Debug/bin/osgGAd.dll
		${THIRD_PARTY_DIR}/osg/Debug/bin/OpenThreadsd.dll
		${THIRD_PARTY_DIR}/osg/Debug/bin/zlibd.dll
		# curl
		${THIRD_PARTY_DIR}/curl/lib/Debug/libcurld.dll
	)

	set(LIBS_RELEASE
		# osg	
		${THIRD_PARTY_DIR}/osg/Release/bin/osg.dll
		${THIRD_PARTY_DIR}/osg/Release/bin/osgDB.dll
		${THIRD_PARTY_DIR}/osg/Release/bin/osgUtil.dll
		${THIRD_PARTY_DIR}/osg/Release/bin/osgViewer.dll
		${THIRD_PARTY_DIR}/osg/Release/bin/osgText.dll
		${THIRD_PARTY_DIR}/osg/Release/bin/osgGA.dll
		${THIRD_PARTY_DIR}/osg/Release/bin/OpenThreads.dll
		${THIRD_PARTY_DIR}/osg/Release/bin/zlib.dll
		# curl
		${THIRD_PARTY_DIR}/curl/lib/Release/libcurl.dll
	)

	# 拷贝依赖的动态库
	add_custom_command(TARGET ${PROJECT_NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"$<$<CONFIG:Debug>:${LIBS_DEBUG}>"
			"$<$<NOT:$<CONFIG:Debug>>:${LIBS_RELEASE}>"
			$<TARGET_FILE_DIR:${PROJECT_NAME}>
		VERBATIM
		COMMENT "Copying dependency libs to ${TARGET_DIR}"
		COMMAND_EXPAND_LISTS
	)
endif()

# 添加 CesiumNative 等三方库的子目录
add_subdirectory(extern)