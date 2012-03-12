# Tala #

Tala is a fork of [Praat](http://www.fon.hum.uva.nl/praat/), intended to port all object-oriented
code from C to C++, modularize the backend into an API separate from the application, and provide a
binding architecture for scripting languages such as Python.

## Installation ##

Currently, only Linux is supported, but the Mac and Windows code shouldn't be too hard to
resurrect.

Requirements:

* GNU Autoconf >= 2.52
* GNU Automake >= 1.6.0
* GNU Libtool  >= 1.4.2
* pkg-config >= 0.24
* GTK+ >= 2.0 (http://www.gtk.org/)
* FLAC (http://flac.sourceforge.net/)
* MAD (http://www.underbit.com/products/mad/)
* PortAudio >= 1.9 (http://www.portaudio.com/)
* GSL (http://www.gnu.org/s/gsl/)
* GLPK >= 4.42 (http://www.gnu.org/s/glpk/)

Steps:

```
$ ./autogen.sh
$ ./configure
$ make
$ make install
```

## Running ##

The executable is located at `src/praat`, and the dynamic library at `src/libpraat.la`.
