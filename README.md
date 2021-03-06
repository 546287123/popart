Building PopART
---------------

These instructions assume you are building PopART on Ubuntu 18.04. These instructions describe how to install every required dependency. If you are starting from an existing Ubuntu 18.04 installation you may already have some of these dependencies installed. If so, please ensure the versions of these dependencies are compatible with these instructions. Other linux-based operating systems may also work but package names and supported versions of packages may vary.

**NOTE**: There is an experimental Dockerfile available in `build_scripts\Dockerfile` which you can use to generate a Docker environment that contains all third-party dependencies you need to compile and run PopART. If you are using this container, please start reading from section "[Installing Graphcore Library Dependencies](#installing-graphcore-library-dependencies)". Note that this approach has been subjected to limited testing.

### Installing Required Tooling

You will need the following tools installed on your system if you have not got them installed already:

**Wget, Git**

```
sudo apt-get install wget git -y
```

##### Python (version 3.6.7 or greater, version 2.x is not supported)

```
sudo apt-get install python3 -y
```

##### PIP3 (package installer for python 3)

```
sudo apt-get install python3-pip -y
ln -s /usr/bin/python3 /usr/bin/python
```

**NOTE**: If you have python 2.x installed on your system you can ignore the symlink.

##### Ninja (version 1.8.2, optional)

```
sudo apt-get install ninja-build -y
```

##### CMake (version 3.10.2 or greater)

Unfortunately, Ubuntu 18.04's default cmake package does not meet the version requirement and hence you have to build cmake from source. Version 3.17.2 is known to work with PopART. To do this, in a directory of your choice, download the source from [here](http://www.cmake.org/download) and build and install cmake as follows:

```
wget https://cmake.org/files/v3.17/cmake-3.17.2.tar.gz
tar xzvf cmake-3.17.2.tar.gz
rm cmake-3.17.2.tar.gz
pushd cmake-3.17.2
./bootstrap --parallel=8 -- -DCMAKE_USE_OPENSSL=OFF
make -j8
sudo make install
popd
```

**NOTE**: The `--parallel=8` and `-j8` switches are used to reduce build times by building with up to 8 threads.

For more information, see: [http://www.cmake.org/download](http://www.cmake.org/download).

### Installing Required PIP Packages

PopART requires the following PIP packages to be installed:

##### ONNX (version 1.7.0 or compatible)

```
sudo pip3 install onnx==1.7.0
```

##### Protobuf (version 3.7.0 or newer)

```
sudo pip3 install protobuf>=3.7.0
```

**NOTE**: The argument `>=3.7.0` is necessary because the python package index may not have version 3.7.0 for the version of python installed on your system.

##### Pytest and Pytest-forked (default versions)

```
sudo pip3 install pytest pytest-forked
```

##### **Numpy** (version 1.19.2 or compatible)

```
sudo pip3 install numpy==1.19.2
```

### Installing Third-Party Library Dependencies

PopART compiles against a number of libraries that you will need to have available on your system:

##### Spdlog (version 0.16.3)

There is a library package in Ubuntun 18.04 that you can use to install this dependency: 

```
sudo apt-get install libspdlog-dev -y
```

##### Pybind11 (version 2.5.0 or compatible)

The version of the pybind11 library in Ubuntu 18.04 (`pybind11-dev`) is 2.0.1, which is not compatible with PopART. Instead, you need to build version 2.5.0 from source. To do this, in a directory of your choice, download the source from [here](https://github.com/pybind/pybind11/releases) and build and install as follows:

```
export PYBIND11_INSTALL_DIR=$(pwd)/pybind11-2.5.0/install_dir/
wget https://github.com/pybind/pybind11/archive/v2.5.0.tar.gz
tar xvfz v2.5.0.tar.gz
rm v2.5.0.tar.gz
pushd pybind11-2.5.0
mkdir build
mkdir install_dir
cd build
cmake .. \
  -DCMAKE_INSTALL_PREFIX=$PYBIND11_INSTALL_DIR \
  -DCMAKE_GENERATOR="Ninja"
ninja
ninja install
popd
```

**NOTE**: If you prefer building with `make` instead of `ninja`, remove the `-DCMAKE_GENERATOR="Ninja"` switch.

**NOTE**: You will need the value of `PYBIND11_INSTALL_DIR` later.

For more information, see: https://github.com/pybind/pybind11/blob/master/docs/compiling.rst.

##### Boost (version 1.70.0 or compatible)

The boost library in Ubuntu 18.04 (`libboost-dev`) is 1.65.1, which is not compatible with PopART. Instead, you have to build version 1.70.0 from source. To do this, in a directory of your choice, download the source from [here](https://www.boost.org/users/history/version_1_70_0.html) and build and install as follows:

```
export BOOST_INSTALL_DIR=$(pwd)/boost_1_70_0/install_dir/
wget https://dl.bintray.com/boostorg/release/1.70.0/source/boost_1_70_0.tar.gz
tar xvfz boost_1_70_0.tar.gz
rm boost_1_70_0.tar.gz
pushd boost_1_70_0
mkdir install_dir
./bootstrap.sh --prefix=$BOOST_INSTALL_DIR
./b2 -j8 link=static runtime-link=static --abbreviate-paths variant=release toolset=gcc "cxxflags= -fno-semantic-interposition -fPIC" cxxstd=14 --with-test --with-system --with-filesystem --with-program_options install
popd
```

**NOTE**: The `-j8` switch is used to reduce build times by building with up to 8 threads.

**NOTE**: You will need the value of `BOOST_INSTALL_DIR` later.

For more information, see: https://www.boost.org/doc/libs/1_70_0/more/getting_started/unix-variants.html.

##### Protobuf (version 3.7.0 or compatible)

The protobuf library in Ubuntu 18.04 (`libprotobuf-dev`) is version 3.0.0. Again, you need to build version 3.7.0 from source. To do this, in a directory of your choice, download the source from [here](https://github.com/protocolbuffers/protobuf/releases) and build and install as follows:

```
export PROTOBUF_INSTALL_DIR=$(pwd)/protobuf-3.6.1/install_dir/
wget https://github.com/protocolbuffers/protobuf/releases/download/v3.6.1/protobuf-cpp-3.6.1.tar.gz 
tar xvfz protobuf-cpp-3.7.0.tar.gz
rm protobuf-cpp-3.7.0.tar.gz
pushd protobuf-3.7.0
mkdir install_dir
CXXFLAGS=-fPIC CFLAGS=-fPIC ./configure \
  --prefix=$PROTOBUF_INSTALL_DIR
make -j8
make check
make install
popd
```

**NOTE**: The `-j8` switch is used to reduce build times by building with up to threads.

**NOTE**: You will need the value of `PROTOBUF_INSTALL_DIR` later.

For more information, see: https://developers.google.com/protocol-buffers/docs/downloads.

##### ONNX (version 1.7.0 or compatible)

The ONNX library also needs to be compiled from source. To do this, in a directory of your choice, download the source from [here](https://github.com/onnx/onnx/releases) and build and install as follows:

```
export ONNX_INSTALL_DIR=$(pwd)/onnx-1.7.0/install_dir/
wget https://github.com/onnx/onnx/archive/v1.7.0.tar.gz
tar xvfz v1.7.0.tar.gz
rm v1.7.0.tar.gz
pushd onnx-1.7.0
mkdir install_dir
cmake .. \
  -DONNX_ML=0 \
  -DProtobuf_PROTOC_EXECUTABLE=$PROTOBUF_INSTALL_DIR/bin/protoc \
  -DCMAKE_INSTALL_PREFIX=$ONNX_INSTALL_DIR
make -j8
make install
popd
```

**NOTE**: The `-j8` switch is used to reduce build times by building with up to 8 threads.

**NOTE**: You will need the value of `ONNX_INSTALL_DIR` later.

For more information, see: https://github.com/onnx/onnx.

### Installing Graphcore Library Dependencies

##### Poprithms

You can checkout Graphcore's poprithms library in a suitable directory from the public [GitHub repository](https://github.com/graphcore/poprithms) and install it as follows:

```
export POPRITHMS_INSTALL_DIR=$(pwd)/poprithms/install_dir/
git clone https://github.com/graphcore/poprithms.git
pushd poprithms
mkdir build; cd build;
cmake .. \
  -DBOOST_ROOT=$BOOST_INSTALL_DIR \
  -DCMAKE_INSTALL_PREFIX=$POPRITHMS_INSTALL_DIR \
  -DCMAKE_GENERATOR="Ninja"
ninja
ninja install
popd
```

**NOTE**: If you prefer building with `make` instead of `ninja`, remove the `-DCMAKE_GENERATOR="Ninja"` switch.

**NOTE**: Builds can be further accelerated by using [ccache](https://ccache.dev/).

**NOTE**: You will need the value of `POPRITHMS_INSTALL_DIR` later.

For more information, see: [https://github.com/graphcore/poprithms](https://github.com/graphcore/poprithms).

##### Poplar SDK

To obtain the Poplar SDK you need to [register](https://www.graphcore.ai/support) for access to Graphcore's [support portal](https://login.graphcore.ai/). Once you have access you can download the latest Ubuntu 18.04 from the support portal, unpack it in a suitable directory. For the remainder of this document the instructions assume you've set an environment variable `POPLAR_INSTALL_DIR` to point to the directory where Poplar is unpacked. Note that the Poplar SDK contains more than just Poplar and you will have to point the variable specifically to a subdirectory named something like `poplar-ubuntu_18_04-xxxxx`.

For more information, see:  https://www.graphcore.ai/developer.

### Configuring & Building PopART

Note that only Ubuntu 18.04 is supported for building PopART externally.

To build PopART, do the following in the directory where you checked out the repository:

```
export POPART_INSTALL_DIR=$(pwd)/popart/install_dir/
git clone https://github.com/graphcore/popart.git
push popart
mkdir build; cd build;
pybind11_DIR=$PYBIND11_INSTALL_DIR cmake .. \
  -DPOPLAR_INSTALL_DIR=$POPLAR_INSTALL_DIR \
  -DPROTOBUF_INSTALL_DIR=$PROTOBUF_INSTALL_DIR \
  -DBOOST_ROOT=$BOOST_INSTALL_DIR \
  -DONNX_INSTALL_DIR=$ONNX_INSTALL_DIR \
  -DPOPRITHMS_INSTALL_DIR=$POPRITHMS_INSTALL_DIR \
  -DCMAKE_INSTALL_PREFIX=$POPART_INSTALL_DIR \
  -DCMAKE_GENERATOR="Ninja"
ninja
ninja install
popd
```

Note that spdlog is picked up by the build system automatically because it is installed as a system package. There is no need to pass spdlog's installation directory to cmake.

**NOTE**: Other cmake switches are available:

* `-DPOPART_BUILD_TESTING=0` - Switch that can be used to avoid compiling PopART test.
* `-DPOPART_BUILD_EXAMPLES=0` - Switch that can be used to avoid compiling PopART examples.
* `-DSPDLOG_INSTALL_DIR=<dir>` - Switch that can be used to point to a specific spdlog library.
* `-DPOPLIBS_INCLUDE_DIR=<dir>`, `-DLIBPVTI_INCLUDE_DIR=<dir>`, etc. - Internal switches that could be used to target alternative internal Poplar libraries.

**NOTE**: If you prefer building with `make` instead of `ninja`, remove the `-DCMAKE_GENERATOR="Ninja"` switch.

**NOTE**: Builds can be further accelerated by using [ccache](https://ccache.dev/).



## Using PopART

##### Running Python Examples

To setup your environment ready for running PopART, do the following:

```
source $POPART_INSTALL_DIR/enable.sh
```

This script will change your `PYTHONPATH` environment variable so that python can find the `popart` module. Now, to run, say, the `simple_addition.py` python example, you can now do the following:

```
python3 $POPART_INSTALL_DIR/examples/python/simple_addition.py
```

**NOTE**: Other python examples are available in the `$POPAR_INSTALL_DIR/examples/python/` directory.

##### Running C++ Examples

To setup your environment ready for running PopART, do the following:

```
source $POPLAR_INSTALL_DIR/enable.sh
source $POPART_INSTALL_DIR/enable.sh
```

This will add Poplar's and PopART's header directories to the `CPATH` environment variable and their shared library directories to `LD_LIBRARY_PATH` environment variable, meaning your compiler and linker can find them. Now you can compile and run the `simple_addition.cpp` example as follows:

```
cd $POPAR_INSTALL_DIR/examples/cplusplus/
g++ -DONNX_NAMESPACE=onnx simple_addition.cpp -o simple_addition -lpoplar -lpopart
./simple_addition
```

**NOTE**: Other python examples are available in the `$POPART_INSTALL_DIR/examples/cplusplus/` directory.

##### Application Examples

There are a number of advanced PopART applications available in Graphcore's [example repository](https://github.com/graphcore/examples/tree/master/applications/popart) on Github.



## Contributing to PopART

##### Coding Style

Please run the `./format.sh` script in the base `popart` directory before making a pull request. This uses `clang-format`
on C++ code and `yapf` on python code. Please use `clang-format ` version 9.0.0  and `yapf` version 0.24. 

**NOTE**: `yapf` can be installed with `pip3`.

##### Unit Tests

Please run the unit test suite in the base `popart` directory to ensure that any changes you have made to the source code have not broken existing functionality:

```
source $POPART_INSTALL_DIR/enable.sh 
cd build
ctest -j 8
```
