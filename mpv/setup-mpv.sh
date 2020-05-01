#!/bin/sh

[ -z "${OE_BASE}" ] && echo "Script require OE_BASE and DISTRO configured by OE environment." && exit 1

curdir=$PWD

echo
echo "Preparing mpv..."
echo

if [ ! -e mpv/Makefile ]; then
	cd ${OE_BASE}/build-${DISTRO} && \
		source env.source && \
		${OE_BASE}/bb/bin/bitbake mpv -cclean && \
		${OE_BASE}/bb/bin/bitbake mpv -cconfigure && {
			cd ${curdir}
			echo
			echo "Copying mpv sources..."
			echo
			cp -R ${OE_BASE}/build-${DISTRO}/tmp/work/*-linux-gnueabi/mpv-*/git/ mpv/
			cp build.sh _env.sh mpv/
			chmod +x mpv/*.sh
			sed -i'' -e "s|\/.*\/git|${curdir}\/mpv|g" ${curdir}/mpv/.lock-waf_*_build
			echo ;echo "--- Setup done ---"; echo
		}
else
	echo
	echo "Nothing to do..."
	echo
fi
