add_library(orchestration-common
  common/protocol.cpp
  common/protocol_heartbeat.cpp
  common/protocol_routing.cpp
  common/csv_log.cpp
  common/time_utils.cpp)

target_include_directories(orchestration-common
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(orchestration-common
  PUBLIC
    Threads::Threads)

add_executable(node-reporter
  node_reporter.cpp
  power_sensor.cpp)

target_compile_definitions(node-reporter
  PRIVATE
    ASIO_STANDALONE)

target_link_libraries(node-reporter
  PRIVATE
    orchestration-common)
