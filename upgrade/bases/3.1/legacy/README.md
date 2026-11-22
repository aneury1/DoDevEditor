## DoDevEditor

this is a simple Text editor with some feature. I create as toy tool and for 
my own use, Im just sharing just if someone one to start from scratchs see 
what to do, for me work as expected. is not production ready text editor in this age
where there we could start any editor and plugins and so on. by now this does not contains
plugins, LSP Server and other nitty gritty details that would do "powerfull" 
it has the minimun I need in my days, probably in the far future I will improve it with 
these feature but by now just enjoy this. 


# Build 

```
Linux
mkdir build && cd build
cmake .. 
cmake --build . -j2
```

Mingw
```
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw32.cmake ..
cmake --build . -j2
```
### CMAKE Config

Remember on CMakeLists.txt set to TRUE the var CMAKE_WIN32_MINGW
and configure cmake with  cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw32.cmake .. 
otherwise linux would be the target.

Compile wxWidget for Mingw32
```sh
git clone --recurse-submodules https://github.com/wxWidgets/wxWidgets.git
cd wxWidgets
mkdir build-mingw32
cd build-mingw32
../configure --host=x86_64-w64-mingw32 --with-msw --prefix=/usr/x86_64-w64-mingw32 --disable-shared 
make -j$(nproc)
sudo make install #(make sure it get installed on prefix path. otherwise it would be a mess.)
### Check installation
ls /usr/x86_64-w64-mingw32/bin
ls /usr/x86_64-w64-mingw32/include/wx #(if this does not appear check ls | grep wx.* , probably you would see something  could be solve by ln -s <wx-version> <wx>)
ls /usr/x86_64-w64-mingw32/lib
```



#### LibGit2
```sh
git clone --recurse-submodules https://github.com/libgit2/libgit2.git
cd libgit2
mkdir build-mingw
cd build-mingw
cmake .. \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_INSTALL_PREFIX=/usr/x86_64-w64-mingw32 \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DCMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32 \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -DBUILD_SHARED_LIBS=OFF \
    -DUSE_SSH=OFF \
    -DUSE_HTTPS=ON \
    -DUSE_OPENSSL=OFF \
    -DUSE_BUNDLED_ZLIB=ON \
    -G Ninja

run compilation
ninja 
or 
make 


then install 
sudo ninja install 
or 
sudo make install
```




### Calling Python-DLT From 

```
sudo apt update
sudo apt install python3-dev

```