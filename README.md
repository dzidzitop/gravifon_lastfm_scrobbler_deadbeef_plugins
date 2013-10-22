Gravifon scrobbler plugin for DeaDBeeF
======================================

Gravifon scrobbler plugin for the audio player DeaDBeeF.

Build instruction (Unix-like systems)
-------------------------------------

Here, `${basedir}` denotes the root directory of the gravifon scrobbler codebase.

1. install the build tool [`ninja`](https://github.com/martine/ninja)
2. install GCC g++ 4.7+
3. install the libraries (including development versions; use your package manager for this):
    * `libjsoncpp`
    * `libcurl`
4. build the static version of the library [`libafc`](https://github.com/dzidzitop/libafc) and copy it to `${basedir}/lib`
5. copy headers of the library `libafc` to `${basedir}/include`
6. get the source code package of DeaDBeeF 0.5.6 and copy the file `deadbeef.h` to `${basedir}/include`
7. execute `ninja sharedLib` from ${basedir}. The shared library `gravifon_scrobbler.so` will be created in `${basedir}/build`
8. copy `${basedir}/build/gravifon_scrobbler.so` to `$HOME/.local/lib/deadbeef`


System requirements
-------------------

* DeaDBeeF 0.5.6
* GCC g++ 4.7+
* libcurl 7.26.0+
* libjsoncpp 0.6.0+
* ninja 0.1.3+
