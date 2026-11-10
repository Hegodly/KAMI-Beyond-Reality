add_executable(KAMIViewer WIN32 main.cpp dx12.cpp dx12.h)
target_compile_definitions(KAMIViewer PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(KAMIViewer PRIVATE d3d12 dxgi user32)
