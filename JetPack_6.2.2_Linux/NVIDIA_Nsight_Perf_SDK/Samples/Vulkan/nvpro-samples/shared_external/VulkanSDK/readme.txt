The Vulkan SDK is not available on aarch64 platform.
So the files under this folder are built from source code.
Here's how to build them on the aarch64 board: 

1. Download the Vulkan SDK Tarball from the official website:
    https://vulkan.lunarg.com/sdk/home#linux

2. Upload the package and extract the package onto board.
For instance:
    tar xvf vulkansdk-linux-x86_64-1.3.239.0.tar.gz

3. Change directory to the extracted folder:
    cd 1.3.239.0

3. Open the script vulkansdk with a text editor and remove
g++-multilib in function install_deps. This library is
not available on aarch64, and it's not needed. 

4. Build the SDK by running script vulkansdk:
    ./vulkansdk
You must have sudo permission because this script installs
build dependencies using command 'sudo apt-get install'.

5. When the build finishes, you can find the binaries under
aarch64 folder.

