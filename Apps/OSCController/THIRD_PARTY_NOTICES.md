# Distribution notices

The controller source and SpatialFabric artwork use the repository's MIT license;
see LICENSE. There is no Unreal Engine runtime or Epic template content in this app.

Source execution uses your installed Python and Tcl/Tk under their own licenses.
The portable Windows build bundles Python, Tcl/Tk, and the PyInstaller bootloader.
They retain their original licensing; this does not relicense them as MIT:

- Python: Python Software Foundation license and historical notices.
- Tcl/Tk: upstream permissive license and copyright notices.
- PyInstaller: GPL-2.0-or-later with its bootloader/distribution exception; see the
  complete COPYING notice. The exception permits distribution of bundled apps.

The Windows build script includes complete runtime notices inside the executable
and in `runtime-licenses/` in the portable ZIP. Distribute the ZIP with its notices.
No third-party OSC library, samples, analytics, cloud service, or local credentials
are included. The build tool itself is not a runtime Python dependency.
