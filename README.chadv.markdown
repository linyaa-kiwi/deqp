drawElements Quality Program
============================

An OpenGL ES and EGL testsuite, primarily used by Android and Chrome OS.


Android
-------
The canonical repository resides in the Android tree.

- Upstream: `git clone https://android.googlesource.com/platform/external/deqp`
- [Gitweb](https://android.googlesource.com/platform/external/deqp)
- [Homepage](https://source.android.com/devices/graphics/testing.html)


Chrome OS
---------
Chrome OS maintains a mirror of the Android repository; a [Portage] [2] ebuild
with additional patches; [Autotest] [1] integration; and hardware-specific
lists of expected results.

- Upstream: `git clone https://chromium.googlesource.com/external/deqp/`
- [Gitweb](https://chromium.googlesource.com/external/deqp/)
- [Ebuild](https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/master/media-gfx/deqp/)
- [Autotest Files](https://chromium.googlesource.com/chromiumos/third_party/autotest/+/master/client/site_tests/graphics_dEQP/)
- [Expectation Lists](https://chromium.googlesource.com/chromiumos/third_party/autotest/+/master/client/site_tests/graphics_dEQP/expectations/)


Chad's Fork
-----------
Chad's fork contains the Chrome OS patchset and some minor changes for Linux
development. The main branch is `chadv`.

- `git clone -b chadv git://github.com/chadversary/deqp`
- [Homepage](https://github.com/chadversary/deqp/blob/chadv/README.chadv.markdown)

For build and installation instructions, see INSTALL.chadv.makrdown.

A snapshot of Chrome OS's Autotest files for dEQP, including the expectation
lists, is maintained in directory `chromiumos-autotest`.



[1]: http://autotest.github.io/
[2]: https://en.wikipedia.org/wiki/Portage_(software)
