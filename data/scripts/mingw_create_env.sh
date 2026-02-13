# Run in: C:\msys64\mingw64.exe

# Enable bash 'strict mode'
set -euo pipefail
IFS=$'\n\t'

echo "Building Libdragon Env..."
cd ~

zipfile="gcc-toolchain-mips64-win64.zip"
sdkpath="/pyrite64-sdk"

export N64_INST=$sdkpath

pacman -S unzip base-devel mingw-w64-x86_64-gcc mingw-w64-x86_64-make git --noconfirm --needed

# download libdragon toolchain
if [ -e $zipfile ]; then
    echo "Toolchain already downloaded"
else
    wget "https://github.com/DragonMinded/libdragon/releases/download/toolchain-continuous-prerelease/gcc-toolchain-mips64-win64.zip" -O $zipfile
fi

if [ -e $sdkpath ]; then
    echo "Toolchain already extracted"
else
    # rm -rf $sdkpath
    unzip $zipfile -d $sdkpath
fi

# Sanity check: Verify that gcc exists
if [ ! -x "$sdkpath/bin/mips64-elf-gcc.exe" ]; then
    echo "ERROR: Toolchain installation appears incomplete."
    echo "Expected file not found: $sdkpath/bin/mips64-elf-gcc.exe"
    echo "Toolchain ZIP file may be corrupted or only partially downloaded."

    rm -rf "$zipfile" "$sdkpath"

    echo "Please re-run the setup to download and extract the toolchain again."
    exit 1
fi

echo "Toolchain installation looks OK."

# download libdragon itself
if [ -e "libdragon" ]; then
    echo "Libdragon already installed, update"
else
    git clone -b preview https://github.com/DragonMinded/libdragon.git
fi

# install libdragon
cd libdragon

git checkout preview
git pull

./build.sh

cd ..

# install tiny3d
if [ -e "tiny3d" ]; then
    echo "Tiny3D already installed, update"
else
    git clone https://github.com/HailToDodongo/tiny3d.git
fi

cd tiny3d

git pull
make clean

# Build Tiny3D
echo "Building Tiny3D..."
make -j6
make install || sudo -E make install

# Tools
echo "Building tools..."
make -C tools/gltf_importer -j6
make -C tools/gltf_importer install || sudo -E make -C tools/gltf_importer install

cd ..

echo "Installation complete!"
sleep 2