add_executable(KAMIViewer WIN32 main.cpp dx12.cpp dx12.h world/region.h world/terrain.h world/terrain.cpp world/region_protocol.h world/region_protocol.cpp)
target_compile_definitions(KAMIViewer PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(KAMIViewer PRIVATE d3d12 dxgi user32)
