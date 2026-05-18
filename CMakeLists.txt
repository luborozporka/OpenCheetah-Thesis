cmake_minimum_required(VERSION 3.11)
project(Cheetah)
include(CMakeFindDependencyMacro)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_compile_options(-fPIC)
add_compile_options(-D_GLIBCXX_USE_CXX11_ABI=0)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

## Dependencies
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Eigen3 3.3.3 REQUIRED)
find_package(SEAL REQUIRED)
find_package(emp-tool REQUIRED)
find_package(emp-ot REQUIRED)
include_directories(${EIGEN3_INCLUDE_DIR}
                    ${EMP-OT_INCLUDE_DIRS} 
                    ${EMP-TOOL_INCLUDE_DIRS}
                    deps/asio/asio/include)

## cheetah-core
add_library(gemini SHARED)
include_directories(include)
add_subdirectory(include)
target_link_libraries(gemini SEAL::seal Eigen3::Eigen Threads::Threads)

if (USE_APPROX_RESHARE)
  target_compile_definitions(gemini PUBLIC USE_APPROX_RESHARE=1)
else()
  target_compile_definitions(gemini PUBLIC USE_APPROX_RESHARE=0)
endif()

## SCI
add_subdirectory(SCI)
include_directories(SCI/src/)

## demo
macro (add_network_cheetah _name)
  set(_sources "networks/main_${_name}.cpp")
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/networks/${_name}_inference.cpp")
    list(APPEND _sources
         "networks/${_name}_inference.cpp"
         "networks/network_globals.cpp")
  endif()
  add_executable(${_name}-cheetah ${_sources})
  target_link_libraries(${_name}-cheetah gemini SCI-Cheetah)
  target_compile_definitions(${_name}-cheetah PUBLIC SCI_OT=1 USE_CHEETAH=1)
endmacro()

macro (add_network_he _name)
  # Prefer the SCI-HE-specific inference source file when present,
  # since DenseNet121's Cheetah implementation differs and hence can't be reused under SCI-HE.
  # This does not apply to other supported networks, namely sqnet and resnet50.
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/networks/${_name}_he_inference.cpp")
    set(_infer_src "networks/${_name}_he_inference.cpp")
  else()
    set(_infer_src "networks/${_name}_inference.cpp")
  endif()
  set(_sources
      "networks/main_${_name}.cpp"
      "${_infer_src}"
      "networks/network_globals.cpp")
  add_executable(${_name}-SCI_HE ${_sources})
  target_link_libraries(${_name}-SCI_HE SCI-HE)
  target_compile_definitions(${_name}-SCI_HE PUBLIC USE_CHEETAH=0)
endmacro()

add_network_cheetah(resnet50)
add_network_cheetah(sqnet)
add_network_cheetah(densenet121)

add_network_he(sqnet)
add_network_he(resnet50)
add_network_he(densenet121)

add_network_cheetah(short1)
add_network_cheetah(short2)
add_network_cheetah(relu1_28_28_1)
add_network_cheetah(relu784)
add_network_cheetah(relu12_23_34_45)
add_network_cheetah(relu422280)
add_network_cheetah(relu1)
add_network_cheetah(mp1)
add_network_cheetah(mp2)
add_network_cheetah(mp3)
add_network_cheetah(mp4)

## Multi-client server
add_executable(server-cheetah
  server/main_server.cpp
  networks/sqnet_inference.cpp
  networks/resnet50_inference.cpp
  networks/densenet121_inference.cpp
  networks/network_globals.cpp)
target_compile_definitions(server-cheetah PRIVATE ASIO_STANDALONE)
target_compile_definitions(server-cheetah PUBLIC SCI_OT=1 USE_CHEETAH=1)
target_link_libraries(server-cheetah gemini SCI-Cheetah Threads::Threads)
