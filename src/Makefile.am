LIBS=`pkg-config --libs gtk+-2.0` -lm -lasound

RANLIB = ls
ICON = # FIXME icons
MAIN_ICON =

SUBDIRS = GSL num num/glpk kar audio audio/FLAC audio/mp3 sys stat fon dwsys dwtools LPC gram FFNet artsynth contrib/ola

AM_CFLAGS = $(COMMON) -I @abs_top_srcdir@/src -std=gnu99
AM_CPPFLAGS = $(COMMON) -I @abs_top_srcdir@/src

bin_PROGRAMS = praat

praat_SOURCES = main/main_Praat.cpp
praat_LDADD = fon/libfon.a contrib/ola/libOla.a \
		LPC/libLPC.a FFNet/libFFNet.a gram/libgram.a dwtools/libdwtools.a \
		artsynth/libartsynth.a fon/libfon.a stat/libstat.a dwsys/libdwsys.a \
		sys/libsys.a num/libnum.a GSL/libgsl.a num/glpk/libglpk.a kar/libkar.a \
		audio/libaudio.a audio/FLAC/libFLAC.a audio/mp3/libmp3.a