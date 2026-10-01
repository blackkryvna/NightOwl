cmake_minimum_required(VERSION 3.16)
project(HoneyDen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Stage 3: Core + Network + Sql (console trap with PostgreSQL writer).
# Widgets/Charts will be added with the GUI stages.
find_package(Qt5 REQUIRED COMPONENTS Core Network Sql)

# NOTE: SOURCES lists only files that really exist.
# New files are added incrementally in later stages.
set(SOURCES
  src/main.cpp
  src/core/Config.cpp
  src/core/EventQueue.cpp
  src/core/Listener.cpp
  src/core/Session.cpp
  src/db/DbWriter.cpp
)

add_executable(honeyden ${SOURCES})
target_include_directories(honeyden PRIVATE src)
target_link_libraries(honeyden PRIVATE Qt5::Core Qt5::Network Qt5::Sql)
