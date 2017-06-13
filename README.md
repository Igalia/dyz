dyz
===

This is just a simple launcher for WPE port of WebKit.

Requirements
------------

* **dyz** is written in Lua and you need LuaJIT to build it (`apt-get install libluajit-5.1-dev`).
* You need Weston or a Wayland session to run **dyz** (`apt-get install weston`).

Build
-----

To build **dyz** you just need to use the following line:

```
user@/path/to/dyz$ make
```

You need also to build WPE port in your WebKit clone:

```
user@/path/to/WebKit$ Tools/Scripts/update-webkitwpe-libs
user@/path/to/WebKit$ Tools/Scripts/build-webkit --wpe
```

Run
---

To run **dyz** you have you have to run Weston first (if you don't have a Wayland session already):

```
$ weston
```

Then you have 2 options:

* From your WebKit path:

```
user@/path/to/WebKit$ Tools/jhbuild/jhbuild-wrapper --wpe run /path/to/dyz/launch -b $(pwd)/WebKitBuild/Release
```

* From **dyz** path:

  * Set `LD_LIBRARY_PATH`:

    ```
    user@/path/to/dyz$ export LD_LIBRARY_PATH=/path/to/WebKit/WebKitBuild/DependenciesWPE/Root/lib
    ```

  * Run **dyz**:

    ```
    user@path/to/dyz$ ./launch -b /path/to/WebKit/WebKitBuild/Release/
    ```
