vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/titanfyre/Titan-Network-Filter/releases/download/sdk-v0.1.1/titan-network-filter-sdk-0.1.1.tar.gz"
    FILENAME "titan-network-filter-sdk-0.1.1.tar.gz"
    SHA512 428999e6cac50a5d8517e5255b9b52b0ebcbe8c112d46902a77f13ddd7a60159b933ba1913fe25fc2e7cf2e50cfc7ab6c285570055a63ff15220e2dd3967ef93
)
vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE "${ARCHIVE}" NO_REMOVE_ONE_LEVEL)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME TitanNetworkFilterSDK CONFIG_PATH lib/cmake/TitanNetworkFilterSDK)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
