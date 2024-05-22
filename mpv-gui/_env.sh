
NUC=${NUC:=1}

SYSROOT_NATIVE=$(readlink -f "sysroot-native")

if [ "$NUC" = "1" ]; then
    export CROSS_COMPILE=x86_64-mobiaqua-linux-
    export CPU_FLAGS="-march=x86-64-v3 -m64"
    export SYSROOT=$(readlink -f "sysroot-x86")
    export PATH=${SYSROOT_NATIVE}/usr/bin/:${SYSROOT_NATIVE}/usr/bin/x86_64-mobiaqua-linux:${PATH}
else
    export CROSS_COMPILE=arm-mobiaqua-linux-gnueabi-
    export CPU_FLAGS="-march=armv7-a -mthumb -mfpu=neon -mfloat-abi=hard"
    export SYSROOT=$(readlink -f "sysroot-arm")
    export PATH=${SYSROOT_NATIVE}/usr/bin/:${SYSROOT_NATIVE}/usr/bin/arm-mobiaqua-linux-gnueabi:${PATH}
fi
