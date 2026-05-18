set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

macro (add_network_OT _name)
  add_executable(${_name}-SCI_OT "main_${_name}.cpp")
  target_link_libraries(${_name}-SCI_OT SCI-OT) 
  target_compile_definitions(${_name}-SCI_OT PUBLIC SCI_OT=1 USE_CHEETAH=0)
endmacro()

macro (add_network_HE _name)
  add_executable(${_name}-SCI_HE "main_${_name}.cpp")
  target_link_libraries(${_name}-SCI_HE SCI-HE) 
  target_compile_definitions(${_name}-SCI_HE PUBLIC SCI-OT=0 USE_CHEETAH=0)
endmacro()

# Networks

# sqnet-SCI_HE, resnet50-SCI_HE, and densenet121-SCI_HE are built from the inference
# source files under networks/ (see add_network_he in the root CMakeLists.txt).
# DenseNet121's Cheetah inference source differs and hence cannot be shared,
# so the HE build uses a dedicated networks/densenet121_he_inference.cpp.

# add_network_HE(sqnet)
# add_network_HE(resnet50)
# add_network_HE(densenet121)
add_network_HE(short1)
add_network_HE(short2)
