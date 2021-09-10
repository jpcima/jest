cmake_minimum_required(VERSION "3.7.0")

project(jest)

###
include(GNUInstallDirs)

###
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

###
find_package(PkgConfig)
find_package(Threads REQUIRED)
find_package(Qt5 COMPONENTS Widgets Network REQUIRED)
pkg_check_modules(JACK "jack" REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBLO "liblo" REQUIRED IMPORTED_TARGET)
pkg_check_modules(GIO "gio-2.0" REQUIRED IMPORTED_TARGET)
find_library(DL_LIBRARY "dl")

###
add_library(nsm INTERFACE)
target_include_directories(nsm INTERFACE "thirdparty/nonlib")
target_link_libraries(nsm INTERFACE PkgConfig::LIBLO)

###
add_library(QProgressIndicator STATIC EXCLUDE_FROM_ALL
  "thirdparty/QProgressIndicator/QProgressIndicator.cpp"
  "thirdparty/QProgressIndicator/QProgressIndicator.h")
target_include_directories(QProgressIndicator PUBLIC "thirdparty/QProgressIndicator")
target_link_libraries(QProgressIndicator PUBLIC Qt5::Widgets)
set_target_properties(QProgressIndicator PROPERTIES
  AUTOMOC TRUE)

###
add_executable(jest
  "sources/jest.cpp"
  "sources/jest_app.cpp"
  "sources/jest_app.h"
  "sources/jest_settings_panel.cpp"
  "sources/jest_settings_panel.h"
  "sources/jest_settings_panel.ui"
  "sources/jest_dsp.cpp"
  "sources/jest_dsp.h"
  "sources/jest_parameters.cpp"
  "sources/jest_parameters.h"
  "sources/jest_worker.cpp"
  "sources/jest_worker.h"
  "sources/jest_client.cpp"
  "sources/jest_client.h"
  "sources/jest_file_helpers.cpp"
  "sources/jest_file_helpers.h"
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

target_link_libraries(jest PRIVATE nsm PkgConfig::JACK Threads::Threads)
target_link_libraries(jest PRIVATE QProgressIndicator Qt5::Widgets Qt5::Network PkgConfig::GIO)
if(DL_LIBRARY)
  target_link_libraries(jest PRIVATE "${DL_LIBRARY}")
endif()

###
install(TARGETS jest DESTINATION "${CMAKE_INSTALL_BINDIR}")
