# Intel® Video Processing Library (Intel® VPL) GPU Runtime

Intel® Video Processing Library (Intel® VPL) GPU Runtime is a Runtime implementation of [Intel® VPL](https://github.com/intel/libvpl/)
API for Intel Gen GPUs. Runtime provides access to hardware-accelerated video decode, encode and filtering.

**Supported video encoders**: HEVC, AVC, MPEG-2, JPEG, VP9  
**Supported video decoders**: HEVC, AVC, VP8, VP9, MPEG-2, VC1, JPEG, AV1, VVC  
**Supported video pre-processing filters**: Color Conversion, Deinterlace, Denoise, Resize, Rotate, Composition  

Implementation is written in C++ 14 with parts in C-for-Media (CM).

Intel® VPL GPU Runtime is a part of Intel software stack for graphics:
* [Linux Graphics Drivers](https://intel.com/linux-graphics-drivers) - General Purpose GPU Drivers for Linux Operating Systems
  * Visit [documentation](https://dgpu-docs.intel.com) for instructions on installing, deploying, and updating Intel software to enable general purpose GPU (GPGPU) capabilities for Linux-based operating system distributions.

Intel® VPL Library and Intel® VPL GPU Runtime are successors for Intel [Media SDK](https://github.com/Intel-Media-SDK/MediaSDK).

# How to use

This runtime implementation is not self-sufficient. Application should use
one of the available frontend dispatcher libraries:

* [Intel® VPL](https://github.com/intel/libvpl/)
* [Media SDK](https://github.com/Intel-Media-SDK/MediaSDK)

We strongly recommend to use [Intel® VPL](https://github.com/intel/libvpl/)
as a dispatcher frontend (libvpl.so.2). [Media SDK](https://github.com/Intel-Media-SDK/MediaSDK)
dispatcher (libmfx.so.1) can be used as well, but you will get capabilities limited by those which
are exposed via Media SDK API and which are actually implemented by Intel® VPL
Runtime. Basically, compatibility with Media SDK is provided to support
Media SDK based applications which did not yet migrate to Intel® VPL on a new HW
platforms.

Intel® VPL Gen Runtime implementation supports the following hardware platforms:

- TGL/RKL/ADL/RPL (TGL: Tiger Lake, RKL: Rocket Lake, ADL-S/P/N: Alder Lake, RPL-S/P: Raptor Lake)
- DG1
- Alchemist(DG2)/ATSM
- MTL/ARL (MTL: Meteor Lake, ARL-S/H: Arrow Lake)
- LNL (Lunar Lake)

# Dependencies
Intel® VPL GPU Runtime depends on [LibVA](https://github.com/intel/libva/).
This version of Intel® VPL GPU Runtime is compatible with the open source [Intel Media Driver for VAAPI](https://github.com/intel/media-driver).
Dispatcher and Samples code hosted in [Intel® VPL](https://github.com/intel/libvpl/) repository.

# Table of contents

  * [License](#license)
  * [How to contribute](#how-to-contribute)
  * [System requirements](#system-requirements)
  * [How to build](#how-to-build)
    * [Build steps](#build-steps)
    * [Enabling Instrumentation and Tracing Technology (ITT)](#enabling-instrumentation-and-tracing-technology-itt)
  * [Recommendations](#recommendations)
  * [See also](#see-also)

# License
Intel® VPL GPU Runtime is licensed under MIT license. See [LICENSE](./LICENSE) for details.

# How to contribute
See [CONTRIBUTING](./CONTRIBUTING.md) for details. Thank you!

# System requirements

**Operating System:**
* Linux x86-64 fully supported
* Linux x86 only build

**Software:**
* [LibVA](https://github.com/intel/libva)
* VAAPI backend driver:
  * [Intel Media Driver for VAAPI](https://github.com/intel/media-driver)
* Some features require CM Runtime library (part of [Intel Media Driver for VAAPI](https://github.com/intel/media-driver) package)

**Hardware:** Intel platforms supported by the [Intel Media Driver for VAAPI](https://github.com/intel/media-driver) starting with Tiger Lake.

# How to build

## Build steps

Get sources with the following Git* command (pay attention that to get full Intel® VPL GPU Runtime sources bundle it is required to have Git* with [LFS](https://git-lfs.github.com/) support):
```sh
git clone https://github.com/intel/vpl-gpu-rt vpl-gpu-rt
cd vpl-gpu-rt
```

To configure and build Intel® VPL GPU Runtime install cmake version 3.14 or later and run the following commands:
```sh
mkdir build && cd build
cmake ..
make
make install
```
Intel® VPL GPU Runtime depends on a number of packages which are identified and checked for the proper version during configuration stage. Please, make sure to install these packages to satisfy Intel® VPL GPU Runtime requirements. After successful configuration 'make' will build Intel® VPL GPU Runtime binaries and samples. The following cmake configuration options can be used to customize the build:

| Option | Values | Description |
| ------ | ------ | ----------- |
| ENABLE_ITT | ON\|OFF | Enable ITT (VTune) instrumentation support (default: OFF) |
| ENABLE_TEXTLOG | ON\|OFF | Enable textlog trace support (default: OFF) |
| ENABLE_STAT | ON\|OFF | Enable stat trace support (default: OFF) |
| BUILD_ALL | ON\|OFF | Build all the BUILD_* targets below (default: OFF) |
| BUILD_RUNTIME | ON\|OFF | Build Intel® VPL runtime (default: ON) |
| BUILD_TESTS | ON\|OFF | Build unit tests (default: OFF) |
| USE_SYSTEM_GTEST | ON\|OFF | Use system gtest version instead of bundled (default: OFF) |
| BUILD_TOOLS | ON\|OFF | Build tools (default: OFF) |
| MFX_ENABLE_KERNELS | ON\|OFF | Build Intel® VPL with [media shaders](https://github.com/Intel-Media-SDK/MediaSDK/wiki/Media-SDK-Shaders-(EU-Kernels)) support (default: ON) |


The following cmake settings can be used to adjust search path locations for some components Intel® VPL GPU Runtime build may depend on:

| Setting | Values | Description |
| ------- | ------ | ----------- |
| CMAKE_ITT_HOME | Valid system path | Location of ITT installation, takes precendence over CMAKE_VTUNE_HOME (by default not defined) |
| CMAKE_VTUNE_HOME | Valid system path | Location of VTune installation (default: /opt/intel/vtune_amplifier) |

## Enabling Instrumentation and Tracing Technology (ITT)

To enable the Instrumentation and Tracing Technology (ITT) API you need to:
* Either install [Intel® VTune™ Amplifier](https://software.intel.com/en-us/intel-vtune-amplifier-xe)
* Or manually build an open source version (see [ITT API](https://github.com/intel/ittapi) for details)

and configure Intel® VPL GPU Runtime with the -DENABLE_ITT=ON. In case of VTune it will be searched in the default location (/opt/intel/vtune_amplifier). You can adjust ITT search path with either CMAKE_ITT_HOME or CMAKE_VTUNE_HOME.

Once Intel® VPL GPU Runtime was built with ITT support, enable it in a runtime creating per-user configuration file ($HOME/.mfx_trace) or a system wide configuration file (/etc/mfx_trace) with the following content:
```sh
Output=0x10
```
# Recommendations

* In case of GCC compiler it is strongly recommended to use GCC version 6 or later since that's the first GCC version which has non-experimental support of C++14 being used in Intel® VPL GPU Runtime.
