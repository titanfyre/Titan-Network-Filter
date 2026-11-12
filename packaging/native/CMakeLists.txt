cmake_minimum_required(VERSION 3.24)
project(TitanNetworkFilterSDK VERSION 0.1.1 LANGUAGES C CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "This SDK requires Windows x64. No driver is installed by this build.")
endif()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
add_library(TitanTitanNetworkFilter STATIC
  "sdk/SDK/NetworkFilter.cpp"
)
add_library(TitanNetworkFilterSDK::sdk ALIAS TitanTitanNetworkFilter)
set_target_properties(TitanTitanNetworkFilter PROPERTIES EXPORT_NAME sdk)
target_compile_features(TitanTitanNetworkFilter PUBLIC cxx_std_17)
target_compile_definitions(TitanTitanNetworkFilter PUBLIC )
target_compile_definitions(TitanTitanNetworkFilter PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
if(MSVC)
  target_compile_options(TitanTitanNetworkFilter PRIVATE /utf-8 /EHsc /W4)
endif()
target_include_directories(TitanTitanNetworkFilter PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk>"
  "$<INSTALL_INTERFACE:include/>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk/SDK>"
  "$<INSTALL_INTERFACE:include/SDK>"
)
target_link_libraries(TitanTitanNetworkFilter PUBLIC advapi32 comctl32 comdlg32 crypt32 fwpuclnt gdi32 newdev ole32 rpcrt4 setupapi shell32 user32 wintrust ws2_32)
install(TARGETS TitanTitanNetworkFilter EXPORT TitanNetworkFilterSDKTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY sdk/ DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl")
install(FILES LICENSE DESTINATION share/titan-network-filter)
install(EXPORT TitanNetworkFilterSDKTargets NAMESPACE TitanNetworkFilterSDK:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TitanNetworkFilterSDK)
configure_file("TitanNetworkFilterSDKConfig.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/TitanNetworkFilterSDKConfig.cmake" @ONLY)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/TitanNetworkFilterSDKConfigVersion.cmake" VERSION 0.1.1 COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/TitanNetworkFilterSDKConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/TitanNetworkFilterSDKConfigVersion.cmake" DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TitanNetworkFilterSDK)
