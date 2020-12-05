#!/bin/sh

[ -z "${OE_BASE}" ] && echo "Script require OE_BASE, DISTRO and TARGET configured by Yocto environment." && exit 1

curdir=$PWD

echo
echo "Preparing MobiAqua toolchain and SDK..."
echo

cd ${OE_BASE}/build-${DISTRO}-${TARGET} && \
	source env.source && \
	${OE_BASE}/bitbake/bin/bitbake external-sdk -cclean && \
	${OE_BASE}/bitbake/bin/bitbake external-sdk -cconfigure && {
		cd ${curdir}
		echo
		echo "Copying..."
		echo
		rm -rf sysroot-native sysroot
		cp -R ${OE_BASE}/build-${DISTRO}-${TARGET}/tmp/work/*-linux-gnueabi/external-sdk/*/recipe-sysroot-native sysroot-native/
		cp -R ${OE_BASE}/build-${DISTRO}-${TARGET}/tmp/work/*-linux-gnueabi/external-sdk/*/recipe-sysroot sysroot/
	} && \
	${OE_BASE}/bitbake/bin/bitbake external-sdk -cclean && {
		echo
		echo "--- Setup done ---"
		echo
	}
