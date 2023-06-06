#!/bin/sh

if [ ! -z "$MSETTINGS" ]; then
	DEFINES="-DHAVE_LIBMSETTINGS"
	LIBS="-lmi_sys -lmi_ao -lmi_gfx -lcam_os_wrapper -lmsettings"
else
	DEFINES=""
	LIBS="-lmi_sys -lmi_ao -lmi_gfx -lcam_os_wrapper"
fi

export CC="${CROSS_COMPILE}gcc"
export AR="${CROSS_COMPILE}gcc-ar"
export RANLIB="${CROSS_COMPILE}gcc-ranlib"
export CFLAGS="-O3 -marm -march=armv7ve+simd -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard\
 -flto -fipa-pta -fipa-ra -ftree-vectorize -ffast-math -funsafe-math-optimizations\
 -fno-math-errno -fno-unwind-tables -fno-asynchronous-unwind-tables\
 -fdata-sections -ffunction-sections -Wl,--gc-sections ${DEFINES}"
export LDFLAGS="${CFLAGS} ${LIBS}"
export CPPFLAGS="${CFLAGS}"
# NOTE: LIBS doesn't seem to be used so I had to put the libs in the LDFLAGS
./configure --host=arm-linux-gnueabihf\
 --enable-alsa=no\
 --enable-alsa-shared=no\
 --enable-esd=no\
 --enable-esd-shared=no\
 --enable-sndio=no\
 --enable-pulseaudio=no\
 --enable-pulseaudio-shared=no\
 --enable-arts=no\
 --enable-arts-shared=no\
 --enable-nas=no\
 --enable-nas-shared=no\
 --enable-diskaudio=no\
 --enable-dummyaudio=no\
 --enable-mintaudio=no\
 --enable-nasm=no\
 --enable-altivec=no\
 --enable-video-x11=no\
 --enable-x11-shared=no\
 --enable-dga=no\
 --enable-video-dga=no\
 --enable-video-x11-dgamouse=no\
 --enable-video-x11-vm=no\
 --enable-video-x11-xv=no\
 --enable-video-x11-xinerama=no\
 --enable-video-x11-xme=no\
 --enable-video-x11-xrandr=no\
 --enable-video-photon=no\
 --enable-video-cocoa=no\
 --enable-video-directfb=no\
 --enable-video-ps2gs=no\
 --enable-video-ps3=no\
 --enable-video-svga=no\
 --enable-video-vgl=no\
 --enable-video-wscons=no\
 --enable-xbios=no\
 --enable-gem=no\
 --enable-video-dummy=no\
 --enable-osmesa-shared=no\
 --enable-input-tslib=no\
 --enable-stdio-redirect=no\
 --enable-directx=no\
 --enable-atari-ldg=no
