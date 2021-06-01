cmake_minimum_required(VERSION "3.7.0")

project(jest)

###
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

###
find_package(PkgConfig)
find_package(Threads REQUIRED)
find_package(Qt5 COMPONENTS Widgets Network REQUIRED)
pkg_check_modules(JACK "jack" REQUIRED IMPORTED_TARGET)
find_library(DL_LIBRARY "dl")

###
add_executable(jest
  "sources/jest.cpp"
  "sources/jest_app.cpp"
  "sources/jest_app.h"
  "sources/jest_dsp.cpp"
  "sources/jest_dsp.h"
  "sources/jest_worker.cpp"
  "sources/jest_worker.h"
  "sources/jest_client.cpp"
  "sources/jest_client.h"
  "sources/jest_main_window.ui"
  "sources/utility/logs.cpp"
  "sources/utility/logs.h"
  "sources/faust/MyQTUI.h"
  "sources/faust/MyQTUI.cpp"
  "resources/resources.qrc")
set_target_properties(jest PROPERTIES
  AUTOMOC TRUE
  AUTORCC TRUE
  AUTOUIC TRUE)

target_link_libraries(jest PRIVATE PkgConfig::JACK Threads::Threads)
target_link_libraries(jest PRIVATE Qt5::Widgets Qt5::Network)
if(DL_LIBRARY)
  target_link_libraries(jest PRIVATE "${DL_LIBRARY}")
endif()
