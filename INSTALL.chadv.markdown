dEQP Configuration and Building
===============================

Choose a target platform
------------------------
A dEQP "target" is a combination of operating system and window system. The
target must be chosen at buildtime, not runtime.  To list the supported
targets, run:

    $ ls targets

Some key targets for Linux are:

  - *x11_egl*: Best choice for running dEQP with X11.

  - *drm*: Best choice for running dEQP in headless mode. This target creates
    no windows and instead runs all tests in a GL framebuffer. It requires an
    EGL implementation that supports [EGL_MESA_configless_context] [1].

[1]: http://cgit.freedesktop.org/mesa/mesa/tree/docs/specs/MESA_configless_context.spec?id=mesa-10.6.0


Configure
---------
Configure the build with CMake:

    $ cmake -DDEQP_TARGET=${TARGET}

You can run dEQP directly from its build directory without installation. If you
plan to install dEQP, then consider setting the additional CMake options below.

 * `CMAKE_INSTALL_PREFIX` [default=`/usr/local`]

 * `DEQP_INSTALL_OPTDIR` [default=`opt/deqp`]

   dEQP will install into `${CMAKE_INSTALL_PREFIX}/${DEQP_INSTALL_OPTDIR}` if
   `DEQP_INSTALL_OPTDIR` is a relative path. Otherwise it will install into
   `${DEQP_INSTALL_OPTDIR}`.


Build and Install
-----------------
Nothing special here:

    $ make

It's not necessary to install dEQP. You can run it directly from its build
directory. If you do want to install, run:

    $ make install
