add_executable(KAMIViewer WIN32 main.cpp dx12.cpp dx12.h world/region.h world/terrain.h world/terrain.cpp world/terrain_stream.h world/terrain_stream.cpp world/terrain_capture.h world/terrain_capture.cpp world/scene_objects.h world/scene_objects.cpp world/mesh_discovery.h world/mesh_discovery.cpp world/mesh_asset_header.h world/mesh_asset_header.cpp world/mesh_asset_queue.h world/mesh_asset_queue.cpp world/primitive_shapes.h world/primitive_shapes.cpp world/texture_entries.h world/texture_entries.cpp world/texture_asset_queue.h world/texture_asset_queue.cpp world/asset_caps.h world/asset_caps.cpp world/texture_atlas.h world/texture_atlas.cpp world/texture_decode_wic.h world/texture_decode_wic.cpp world/texture_decode_openjpeg.h world/texture_decode_openjpeg.cpp world/region_protocol.h world/region_protocol.cpp world/lludp.h world/lludp.cpp world/land_layer.h world/land_layer.cpp world/simulator_transport.h world/simulator_transport.cpp world/simulator_session.h world/simulator_session.cpp world/login_protocol.h world/login_protocol.cpp world/login_response.cpp world/login_password.cpp world/login_tls.cpp world/login_identity.cpp)
target_compile_definitions(KAMIViewer PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(KAMIViewer PRIVATE d3d12 dxgi user32 ws2_32 comdlg32 winhttp bcrypt ole32 oleaut32 iphlpapi windowscodecs openjp2)
target_include_directories(KAMIViewer PRIVATE
  "${openjpeg_SOURCE_DIR}/src/lib/openjp2"
  "${openjpeg_BINARY_DIR}/src/lib/openjp2")
enable_testing()
add_executable(kami_lludp_tests tests/lludp_tests.cpp world/lludp.cpp)
if(MSVC)
  add_compile_options(/UNDEBUG)
endif()
target_include_directories(kami_lludp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME lludp_packet_parsing COMMAND kami_lludp_tests)
add_executable(kami_transport_tests tests/simulator_transport_tests.cpp world/simulator_transport.cpp world/lludp.cpp)
target_include_directories(kami_transport_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kami_transport_tests PRIVATE ws2_32)
add_test(NAME simulator_transport_parsing COMMAND kami_transport_tests)
add_executable(kami_session_tests tests/simulator_session_tests.cpp world/simulator_session.cpp world/simulator_transport.cpp world/lludp.cpp)
target_include_directories(kami_session_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kami_session_tests PRIVATE ws2_32)
add_test(NAME simulator_session_handshake COMMAND kami_session_tests)

add_executable(kami_terrain_tests tests/terrain_tests.cpp world/terrain.cpp world/region_protocol.cpp world/land_layer.cpp world/lludp.cpp)
target_include_directories(kami_terrain_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME terrain_streaming_and_partial_rendering COMMAND kami_terrain_tests)

add_executable(kami_land_layer_tests tests/land_layer_tests.cpp world/land_layer.cpp world/region_protocol.cpp)
target_include_directories(kami_land_layer_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME land_layer_bitstream COMMAND kami_land_layer_tests)

add_executable(kami_terrain_stream_tests tests/terrain_stream_tests.cpp world/terrain_stream.cpp world/terrain.cpp world/land_layer.cpp world/region_protocol.cpp world/lludp.cpp)
target_include_directories(kami_terrain_stream_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME udp_packet_to_partial_mesh COMMAND kami_terrain_stream_tests)

add_executable(kami_capture_tests tests/terrain_capture_tests.cpp world/terrain_capture.cpp world/terrain_stream.cpp world/terrain.cpp world/land_layer.cpp world/region_protocol.cpp world/lludp.cpp)
target_include_directories(kami_capture_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME offline_real_packet_replay COMMAND kami_capture_tests)

add_executable(kami_login_tests tests/login_tests.cpp world/login_protocol.cpp world/login_password.cpp world/login_response.cpp world/login_tls.cpp world/login_identity.cpp world/simulator_session.cpp world/simulator_transport.cpp world/lludp.cpp)
target_include_directories(kami_login_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kami_login_tests PRIVATE ws2_32 winhttp bcrypt ole32 oleaut32 iphlpapi)
add_test(NAME secure_login_protocol COMMAND kami_login_tests)

# Simulator-compatible fixture: little-endian headers, consecutive patches,
# zigzag dequantization, zero-coding and an actual non-flat decoded surface.
add_executable(kami_real_wire_terrain_tests
  tests/terrain_wire_format_tests.cpp
  world/terrain_stream.cpp world/terrain.cpp world/land_layer.cpp
  world/region_protocol.cpp world/lludp.cpp)
target_include_directories(kami_real_wire_terrain_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME real_second_life_layer_wire_format COMMAND kami_real_wire_terrain_tests)

# Compiles the exact embedded HLSL shaders and checks the row-vector
# camera convention against DirectXMath; does not require a GPU.
add_executable(kami_dx12_projection_tests tests/dx12_projection_tests.cpp)
target_include_directories(kami_dx12_projection_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kami_dx12_projection_tests PRIVATE d3dcompiler)
add_test(NAME dx12_camera_projection_and_shader COMMAND kami_dx12_projection_tests)

# Official ObjectUpdate, ObjectUpdateCompressed, ObjectUpdateCached and KillObject
# binary record fixtures; these are synthetic and contain no resident data.
add_executable(kami_scene_object_tests
  tests/scene_object_tests.cpp world/scene_objects.cpp world/mesh_discovery.cpp world/primitive_shapes.cpp world/texture_entries.cpp world/texture_asset_queue.cpp world/texture_atlas.cpp
  world/terrain.cpp world/region_protocol.cpp world/land_layer.cpp world/lludp.cpp)
target_include_directories(kami_scene_object_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME second_life_scene_object_update_packets COMMAND kami_scene_object_tests)

# Packed LL TextureEntry fields and per-face exception masks. No network or
# image asset loading takes place in this protocol decoder.
add_executable(kami_texture_entry_tests
  tests/texture_entry_tests.cpp world/texture_entries.cpp)
target_include_directories(kami_texture_entry_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME second_life_texture_entry_colors COMMAND kami_texture_entry_tests)

# No network or GPU dependencies. Codec data is staged, not decoded.
add_executable(kami_texture_asset_queue_tests tests/texture_asset_queue_tests.cpp world/texture_asset_queue.cpp)
target_include_directories(kami_texture_asset_queue_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME bounded_jpeg2000_staging_queue COMMAND kami_texture_asset_queue_tests)

# Validate opaque Second Life capability replies and URL construction offline.
# HTTP access is only exercised at runtime with authenticated simulator caps.
add_executable(kami_asset_capability_tests
  tests/asset_capability_tests.cpp world/asset_caps.cpp world/texture_asset_queue.cpp world/mesh_asset_header.cpp)
target_include_directories(kami_asset_capability_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(kami_asset_capability_tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(kami_asset_capability_tests PRIVATE winhttp ole32 oleaut32)
add_test(NAME secure_viewer_asset_capabilities COMMAND kami_asset_capability_tests)

add_executable(kami_texture_atlas_tests tests/texture_atlas_tests.cpp world/texture_atlas.cpp world/texture_asset_queue.cpp)
target_include_directories(kami_texture_atlas_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME texture_atlas_bounds_and_uv COMMAND kami_texture_atlas_tests)

add_executable(kami_wic_image_tests tests/texture_decode_wic_tests.cpp
  world/texture_decode_wic.cpp world/texture_atlas.cpp world/texture_asset_queue.cpp)
target_include_directories(kami_wic_image_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kami_wic_image_tests PRIVATE windowscodecs ole32)
add_test(NAME wic_decoded_pixel_conversion COMMAND kami_wic_image_tests)

# Real JPEG2000 decoding regression fixture, not just a byte-signature test.
add_executable(kami_openjpeg_decode_tests tests/texture_decode_openjpeg_tests.cpp
  world/texture_decode_openjpeg.cpp world/texture_atlas.cpp world/texture_asset_queue.cpp)
target_include_directories(kami_openjpeg_decode_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kami_openjpeg_decode_tests PRIVATE openjp2)
target_include_directories(kami_openjpeg_decode_tests PRIVATE
  "${openjpeg_SOURCE_DIR}/src/lib/openjp2"
  "${openjpeg_BINARY_DIR}/src/lib/openjp2")
add_test(NAME real_openjpeg_j2k_pixel_decode COMMAND kami_openjpeg_decode_tests)

# OpenJPEG's BSD 2-clause license accompanies the statically linked binary.
configure_file("${openjpeg_SOURCE_DIR}/LICENSE"
               "${CMAKE_CURRENT_BINARY_DIR}/OpenJPEG-LICENSE.txt" COPYONLY)

add_executable(kami_mesh_discovery_tests tests/mesh_discovery_tests.cpp
  world/mesh_discovery.cpp world/texture_asset_queue.cpp)
target_include_directories(kami_mesh_discovery_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME mesh_extra_params_bounded_parsing COMMAND kami_mesh_discovery_tests)

add_executable(kami_mesh_header_tests tests/mesh_asset_header_tests.cpp
  world/mesh_asset_header.cpp)
target_include_directories(kami_mesh_header_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME binary_llsd_mesh_header_bounds COMMAND kami_mesh_header_tests)

add_executable(kami_mesh_queue_tests tests/mesh_asset_queue_tests.cpp
 world/mesh_asset_queue.cpp world/mesh_asset_header.cpp world/texture_asset_queue.cpp)
target_include_directories(kami_mesh_queue_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME mesh_asset_staging_queue COMMAND kami_mesh_queue_tests)
