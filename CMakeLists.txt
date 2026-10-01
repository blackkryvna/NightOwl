cmake_minimum_required(VERSION 3.16)
project(HoneyDen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Stage 5: GUI starts (Widgets). Sql still linked for the DB writer.
# Charts joins in stage 7 with the live graph.
find_package(Qt5 REQUIRED COMPONENTS Core Network Sql Widgets)

# libmaxminddb has no CMake package: probe headers/lib manually.
# Missing lib -> build still works, GeoLookup returns empty strings.
find_path(MAXMINDDB_INCLUDE_DIR maxminddb.h)
find_library(MAXMINDDB_LIBRARY maxminddb)

# NOTE: SOURCES lists only files that really exist.
# New files are added incrementally in later stages.
set(SOURCES
  src/main.cpp
  src/core/Config.cpp
  src/core/EventQueue.cpp
  src/core/GeoLookup.cpp
  src/core/Listener.cpp
  src/core/Session.cpp
  src/db/DbWriter.cpp
  src/ui/MainMenu.cpp
)

add_executable(honeyden ${SOURCES})
target_include_directories(honeyden PRIVATE src)
target_link_libraries(honeyden PRIVATE Qt5::Core Qt5::Network Qt5::Sql Qt5::Widgets)
if(MAXMINDDB_INCLUDE_DIR AND MAXMINDDB_LIBRARY)
  target_include_directories(honeyden PRIVATE ${MAXMINDDB_INCLUDE_DIR})
  target_link_libraries(honeyden PRIVATE ${MAXMINDDB_LIBRARY})
  target_compile_definitions(honeyden PRIVATE HAVE_MAXMINDDB)
else()
  message(WARNING "libmaxminddb not found, GeoIP disabled (fields stay empty)")
endif()
