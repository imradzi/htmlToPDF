rmdir /s /q build
mkdir build
cd build
cmake .. -DCMAKE_GENERATOR_PLATFORM=x64 -DVCPKG_ROOT=G:/vcpkg -DVCPKG_TARGET_TRIPLET=x64-windows-static-custom -DCMAKE_TOOLCHAIN_FILE=G:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_PREFIX_PATH=G:/wkhtmltopdf