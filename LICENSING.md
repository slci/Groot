# Licensing and provenance

This is an unofficial fork of [Groot 1](https://github.com/BehaviorTree/Groot).
The Groot 1 sources are distributed under the MIT notice preserved in
[LICENSE](LICENSE) and in the README. The BehaviorTree.CPP v4 support was added
to this fork; it does not include Groot2 source code or assets.

Bundled third-party source:

- `QtNodeEditor/`: BSD 3-Clause; copyright and full terms are in
  [QtNodeEditor/LICENSE](QtNodeEditor/LICENSE).
- `cmake/FindZMQ.cmake`: New BSD; the author's copyright is in that file and
  its referenced terms are in
  [cmake/COPYING-CMAKE-SCRIPTS](cmake/COPYING-CMAKE-SCRIPTS).

External build and runtime dependencies are not vendored here. In particular,
BehaviorTree.CPP v4 is [MIT licensed](https://github.com/BehaviorTree/BehaviorTree.CPP/blob/4.10.0/LICENSE),
and Qt modules have [open-source or commercial terms that vary by module](https://www.qt.io/development/open-source-lgpl-obligations).
ZeroMQ and any other libraries included in a binary package have their own
terms. A binary distributor must check the exact library versions and include
their required license texts and notices. Qt's LGPL distribution rules also
require a prominent notice, a copy of the license, and a way for users to
replace the Qt libraries; see
[Qt's guidance](https://www.qt.io/development/open-source-lgpl-obligations).
If distributing an installer built with Qt Installer Framework, review that
tool's distribution terms for the exact version used.

The inactive `bt_editor/package.xml.disabled` is legacy ROS metadata that says
`LGPLv3`, while the active root `package.xml` says `MIT`. It is not used by this
build. Resolve that historical metadata discrepancy with the original authors
before relying on it to license a separately distributed `bt_editor` package.
