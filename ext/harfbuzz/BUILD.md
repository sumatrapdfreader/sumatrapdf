On Linux, install the development packages for FreeType,
Cairo, and GLib. For example, on Ubuntu / Debian, you would do:

    sudo apt-get install gcc g++ libfreetype6-dev libglib2.0-dev libcairo2-dev

whereas on Fedora, RHEL, CentOS, and other Red Hat based systems you would do:

    sudo yum install gcc gcc-c++ freetype-devel glib2-devel cairo-devel

on Windows, consider using [vcpkg](https://github.com/Microsoft/vcpkg),
provided by Microsoft, for building HarfBuzz and other open-source libraries
but if you need to build harfbuzz from source, put ragel binary on your
PATH and follow appveyor CI's cmake
[build steps](https://github.com/harfbuzz/harfbuzz/blob/master/appveyor.yml).

on macOS, using MacPorts:

    sudo port install freetype glib2 cairo

or using Homebrew:

    brew install freetype glib cairo

If you are using a tarball, you can now proceed to running configure and make
as with any other standard package. That should leave you with a shared
library in `src/`, and a few utility programs including `hb-view` and `hb-shape`
under `util/`.

If you are bootstrapping from git, you need a few more tools before you can
run `autogen.sh` for the first time. Namely, `pkg-config` and `ragel`.

Again, on Ubuntu / Debian:

    sudo apt-get install autoconf automake libtool pkg-config ragel gtk-doc-tools

and on Fedora, RHEL, CentOS:

    sudo yum install autoconf automake libtool pkgconfig ragel gtk-doc

on the Mac, using MacPorts:

    sudo port install autoconf automake libtool pkgconfig ragel gtk-doc

or using Homebrew:

    brew install autoconf automake libtool pkgconfig ragel gtk-doc

To build the Python bindings, you also need:

    brew install pygobject3
