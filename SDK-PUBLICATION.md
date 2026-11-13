# SDK publication status

Current source layout and registry coverage. A GitHub download is distinct from publication on a central registry.

| Target | Status | Package / documentation |
| --- | --- | --- |
| CPP/vcpkg | published_custom_registry | titan-network-filter |
| CPP/conan | public_recipe_builds_locally |  [Source](packaging/native/) |
| CPP/nuget | paused_pending_owner_email_confirmation | Titan-Network-Filter.Native |
| NET/nuget | built_verified_publication_on_hold_by_owner | TitanNetworkFilter.SDK [Source](Titan-Network-Filter-NET-nuget/) |
| Python/pypi | built_fixture_and_production_verified | titan-network-filter [Source](Titan-Network-Filter-Python-pypi/) |
| Python/conda-forge | built_tested_requires_source_publication_and_official_review | titan-network-filter [Source](packaging/conda/) |
| Rust/cargo | published_install_verified | [titan-network-filter](https://crates.io/crates/titan-network-filter) [Source](Titan-Network-Filter-Rust-cargo/) |
| JavaScript/npm | awaiting_browser_publication_authentication | [titan-network-filter](https://www.npmjs.com/package/titan-network-filter) [Source](Titan-Network-Filter-JavaScript-npm/) |
| Java/maven | published_install_verified | [titan-network-filter](https://central.sonatype.com/artifact/io.github.titancsa/titan-network-filter/1.0.0) [Source](Titan-Network-Filter-Java-maven/) |
| Java/jitpack | published_install_verified | [titan-network-filter](https://jitpack.io/#titanfyre/Titan-Network-Filter/java-sdk-v1.0.0) [Source](Titan-Network-Filter-Java-maven/) |
| Go/modules | published_install_verified | [titan-network-filter](https://pkg.go.dev/github.com/titanfyre/Titan-Network-Filter/Titan-Network-Filter-Go-modules@v1.0.0) [Source](Titan-Network-Filter-Go-modules/) |
| Go/pkg-go-dev | indexed_verified | [Documentation](https://pkg.go.dev/github.com/titanfyre/Titan-Network-Filter/Titan-Network-Filter-Go-modules@v1.0.0) |
| PowerShell/gallery | published_install_verified | [TitanNetworkFilter](https://www.powershellgallery.com/packages/TitanNetworkFilter/1.0.0) [Source](Titan-Network-Filter-PowerShell-gallery/) |
| Dart/pub-dev | built_fixture_and_production_verified | titan_network_filter [Source](Titan-Network-Filter-Dart-pub-dev/) |
| Ruby/rubygems | published_install_verified | [titan-network-filter](https://rubygems.org/gems/titan-network-filter) [Source](Titan-Network-Filter-Ruby-rubygems/) |
| PHP/packagist | composer_source_and_tag_published | [titancsa/titan-network-filter](https://packagist.org/packages/titancsa/titan-network-filter) |

Native SDK source and CMake/Conan/vcpkg/NuGet recipes share [`packaging/native`](packaging/native/LAYOUT.md). Previous release archives and tags remain unchanged.

Application and driver prerequisites and original licensing remain those documented by each SDK. No driver installer is bundled with these source packages.
