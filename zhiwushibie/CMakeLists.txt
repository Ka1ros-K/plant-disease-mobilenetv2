cmake_minimum_required(VERSION 3.16)

project(zhiwushibie VERSION 0.1 LANGUAGES CXX)

# -------- Qt 基本配置 --------
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ⚠️ Widgets + Network + LinguistTools + Sql
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network LinguistTools Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network LinguistTools Sql)

set(TS_FILES zhiwushibie_yue_CN.ts)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    resources.qrc
    ${TS_FILES}
    PlantId.h
    PlantId.cpp
    PlantDisease.h
    PlantDisease.cpp
    DeepseekClient.h
    DeepseekClient.cpp       # 别忘了加进来
    DatabaseManager.h
    DatabaseManager.cpp
    HistoryWindow.h
    HistoryWindow.cpp
    HistoryWindow.ui
    SettingsWindow.h
    SettingsWindow.cpp
    SettingsWindow.ui
    InfoWindow.h
    InfoWindow.cpp
    InfoWindow.ui
    ChatWindow.h
    ChatWindow.cpp
    ChatWindow.ui
    CameraWindow.h
    CameraWindow.cpp
    PlantComparisonWindow.h
    PlantComparisonWindow.cpp
    PlantComparisonWindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(zhiwushibie
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(zhiwushibie SHARED ${PROJECT_SOURCES})
    else()
        add_executable(zhiwushibie ${PROJECT_SOURCES})
    endif()
    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

# ⚠️ 链接 Widgets + Network + Sql
target_link_libraries(zhiwushibie PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
)

# -------- 强制使用 MSVC --------
if(NOT MSVC)
    message(FATAL_ERROR "请使用 MSVC 工具链（如 Desktop Qt 6.x MSVC 64-bit）。onnxruntime 官方二进制不支持 MinGW。")
endif()

# -------- OpenCV （用于读图/预处理）--------
if(NOT DEFINED OpenCV_DIR)
    message(STATUS "提示：建议设置 OpenCV_DIR，例如 C:/opencv/build/x64/vc15/lib")
endif()
find_package(OpenCV CONFIG REQUIRED)
target_include_directories(zhiwushibie PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(zhiwushibie PRIVATE ${OpenCV_LIBS})

# -------- ONNX Runtime --------
set(ONNXRUNTIME_ROOT "${CMAKE_SOURCE_DIR}/onnxruntime-win-x64-1.22.1" CACHE PATH "onnxruntime 解压根目录")

target_include_directories(zhiwushibie PRIVATE "${ONNXRUNTIME_ROOT}/include")
target_link_directories(zhiwushibie PRIVATE "${ONNXRUNTIME_ROOT}/lib")
target_link_libraries(zhiwushibie PRIVATE onnxruntime)

# 运行时拷贝 DLL
if (WIN32)
    add_custom_command(TARGET zhiwushibie POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll"
            $<TARGET_FILE_DIR:zhiwushibie>
    )

    # 复制 Qt 平台插件
    add_custom_command(TARGET zhiwushibie POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:zhiwushibie>/platforms
    )
    add_custom_command(TARGET zhiwushibie POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "D:/installed/Qt/6.10.1/msvc2022_64/plugins/platforms/qwindows.dll"
            $<TARGET_FILE_DIR:zhiwushibie>/platforms/
    )

    # 复制 Qt SQL 驱动
    add_custom_command(TARGET zhiwushibie POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:zhiwushibie>/sqldrivers
    )
    add_custom_command(TARGET zhiwushibie POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "D:/installed/Qt/6.10.1/msvc2022_64/plugins/sqldrivers/qsqlite.dll"
            $<TARGET_FILE_DIR:zhiwushibie>/sqldrivers/
    )
endif()

# -------- 应用元信息 --------
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.zhiwushibie)
endif()
set_target_properties(zhiwushibie PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# -------- 资源文件 --------
# 安装资源文件夹到构建目录
install(DIRECTORY resources/
    DESTINATION resources
    FILES_MATCHING
    PATTERN "*.png"
    PATTERN "*.jpg"
    PATTERN "*.jpeg"
    PATTERN "*.bmp"
    PATTERN "*.tiff"
    PATTERN "*.ico"
    PATTERN "*.json"
    PATTERN "*.onnx"
    PATTERN "*.txt"
    PATTERN "*.md"
)

# 运行时拷贝资源文件夹
if (WIN32)
    add_custom_command(TARGET zhiwushibie POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/resources"
            $<TARGET_FILE_DIR:zhiwushibie>/resources
    )
endif()

# -------- 安装规则（可选）--------
include(GNUInstallDirs)
install(TARGETS zhiwushibie
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(zhiwushibie)
endif()
