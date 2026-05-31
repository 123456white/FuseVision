find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql)
find_package(spdlog REQUIRED)

add_subdirectory(core)
add_subdirectory(DeepLearningWidget)
add_subdirectory(TraditionalWidget)
add_subdirectory(MainWindows)
