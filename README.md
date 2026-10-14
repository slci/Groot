# Experimental Groot 1 port for BehaviorTree.CPP v4

This fork adapts the original Groot 1 application to BehaviorTree.CPP v4. It has
been built and tested with BehaviorTree.CPP 4.10.0 and Qt 5.15 on Linux.

Supported paths:

- Edit and save `BTCPP_format="4"` XML, including subtrees, custom node models,
  direct custom node tags, and the existing SVG export and GUI editing tools.
- Monitor a running tree through `BT::Groot2Publisher` on its single TCP server
  port (1667 by default). Tree structure, current status, and short-lived node
  transitions are shown.
- Replay `.btlog` files written by `BT::FileLogger2`.

The port does not implement Groot2's breakpoint, blackboard inspection, or
log capture controls. Groot 1 `.fbl` traces are not accepted; use `FileLogger2`
for v4 logs. Existing v3 XML should be migrated to v4 before loading. When
custom nodes are used directly without a `TreeNodesModel`, the editor shows
their type as Unspecified and preserves their direct XML tags when saving.

# Groot 

**Groot** is a Graphical Editor, written in C++ and Qt, to create [BehaviorTrees](https://en.wikipedia.org/wiki/Behavior_tree).

It is based on the original [Groot](https://github.com/BehaviorTree/Groot)
application and uses [BehaviorTree.CPP](https://github.com/BehaviorTree/BehaviorTree.CPP).

[![Groot Editor](groot-screenshot.png)](https://vimeo.com/275066768)


In the following video you can see how the C++ library and
the graphic user interface are used to design and monitor a Behavior Tree.

[![MOOD2Be](video_MOOD2Be.png)](https://vimeo.com/304651183)


# Dependencies, Installation, and Usage

To compile the project you need CMake 3.16 or newer, a C++17 compiler,
BehaviorTree.CPP v4, Qt5 development files (Core, Widgets, Gui, OpenGL, Xml,
Svg, Test, Concurrent), and ZeroMQ with the `zmq.hpp` and `zmq_addon.hpp`
headers. The monitor is built when ZeroMQ is found.

## Compilation instructions (Linux)

```
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/behaviortree_cpp/v4/install
cmake --build build -j
ctest --test-dir build --output-on-failure
```

If Qt5 is installed on the host and BT.CPP v4 is in a Pixi or Conda
environment, choose the host C and C++ compilers and point CMake at the host
Qt5 package. Avoid linking host Qt5 with a Conda compiler sysroot.

Run `build/Groot --mode editor --file tree.xml` to edit a v4 tree, or
`build/Groot --mode monitor --address 127.0.0.1 --server_port 1667 --autoconnect`
to connect to a `BT::Groot2Publisher`. The old `--publisher_port` option is
accepted for compatibility but has no effect with the v4 protocol.

The default CTest suite covers v4 XML editing, runtime tree parsing, and
`FileLogger2` replay. The original v3 fixture tests can be built with
`-DGROOT_BUILD_LEGACY_TESTS=ON`; they have not been migrated to v4. The live
publisher test can be run with `QT_QPA_PLATFORM=minimal build/test/v4_compat_test
runtimeMonitor` on a host that permits local TCP sockets.

For desktop validation on a Wayland session, run
`GROOT_SAMPLE_TREE=/absolute/path/to/v4-tree.xml GROOT_SCREENSHOT_DIR=/tmp
build/test/v4_compat_test desktopWorkflow`. This opens the editor, subtree,
replay, and live monitor windows and saves four `groot-v4-*.png` screenshots.

# Licence       

Copyright (c) 2018-2019 FUNDACIO EURECAT 

Permission is hereby granted, free of charge, to any person obtaining a 
copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation 
the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included 
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
DEALINGS IN THE SOFTWARE.

  
# Acknowledgment

This project is one of the main components of [MOOD2Be](https://eurecat.org/es/portfolio-items/mood2be/),
and it is developed at [Eurecat](https://eurecat.org) by Davide Faconti.

MOOD2Be is one of the six **Integrated Technical Projects (ITPs)** selected from the [RobMoSys first open call](https://robmosys.eu/itp/). 

It received funding from the European Union’s Horizon 2020 Research and Innovation Programme
under the RobMoSys project.

