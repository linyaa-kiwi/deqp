drawElements Quality Program
============================

This is chadv's fork of dEQP, an OpenGL ES and EGL testsuite, primarily used by Android and Chrome OS.
Go read the upstream [README](https://android.googlesource.com/platform/external/deqp/+/master/README.md) first.

## chadv's dEQP

chadv's fork contains the Chromium OS patchset and some minor changes for Linux
development.

- [Homepage](http://git.kiwitree.net/cgit/~chadv/deqp/about)
- `git clone -b master git://git.kiwitree.org/~chadv/deqp`

For build and installation instructions, see INSTALL.chadv.makrdown.


### Branches and Tags

- **refs/heads/master**  
  My personal fork, which is based on Android's master branch. It includes
  fixes and improvements for Linux and Chromium OS. I semi-regularly merge
  Android's master branch and apply the patches from Chromium OS's dEQP
  package.
  .
  To view my non-upstreamed patches, I recommend:  

        % git remote add aosp https://android.googlesource.com/platform/external/deqp
        % git fetch aosp
        % git log --oneline --decorate ^aosp/master master

- **refs/tags/chromiumos-deqp-$VERSION**  
  A snapshot of the exact dEQP code built by Chromium OS's dEQP package,
  `media-gfx/deqp-${VERSION}.ebuild`. This includes patches applied by the
  ebuild.

- **refs/heads/chromiumos-latest**  
  Points to the latest tag *chromiumos-deqp-$VERSION*.

- **refs/heads/chromiumos-latest-fixes**  
  My fixes, rebased onto branch *chromiumos-latest*.


## Android's dEQP

The canonical repository resides in the Android tree.

- Upstream: `git clone https://android.googlesource.com/platform/external/deqp`
- [Gitweb](https://android.googlesource.com/platform/external/deqp)
- [Homepage](https://source.android.com/devices/graphics/testing.html)


## Chromium OS's dEQP

Chromium OS maintains a mirror of the Android repository; a [Portage] [2] ebuild
with additional patches; [Autotest] [1] integration; and hardware-specific
lists of expected results.

- Upstream: `git clone https://chromium.googlesource.com/external/deqp/`
- [Gitweb](https://chromium.googlesource.com/external/deqp/)
- [Ebuild](https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/master/media-gfx/deqp/)
- [Autotest Files](https://chromium.googlesource.com/chromiumos/third_party/autotest/+/master/client/site_tests/graphics_dEQP/)
- [Expectation Lists](https://chromium.googlesource.com/chromiumos/third_party/autotest/+/master/client/site_tests/graphics_dEQP/expectations/)





[1]: http://autotest.github.io/
[2]: https://en.wikipedia.org/wiki/Portage_(software)
