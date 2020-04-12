#!/bin/sh


[ "${OE_BASE}" == "" ] && echo "Script require OE_BASE and DISTRO configured by OE environment." && exit 1

echo
echo "Preparing mplayer..."
echo
curdir=$PWD
cd ${OE_BASE}/build-${DISTRO} && source env.source && ${OE_BASE}/bb/bin/bitbake mplayer -cclean && ${OE_BASE}/bb/bin/bitbake mplayer -cconfigure && {
	cd ${curdir}
	if [ ! -e mplayer/Makefile ]; then
		echo
		echo "Copying mplayer sources..."
		echo
		if [ ! -e ${OE_BASE}/build-${DISTRO}/tmp/work/*-linux-gnueabi/mplayer-*/trunk/Makefile ]; then
			echo "Error: mplayer OE build dir not exist, you need to disable INHERIT = \"rm_work\"  and try again."
			echo
			exit 1
		fi
		cp -R ${OE_BASE}/build-${DISTRO}/tmp/work/*-linux-gnueabi/mplayer-*/trunk/ mplayer/
	fi
	cp build.sh clean.sh _env.sh mplayer/
	chmod +x mplayer/*.sh
	echo ;echo "--- Setup done ---"; echo
}
