compile(){
    export USE_CCACHE=1
    export CCACHE_DIR=~/.ccache
    #PATH=/home/chrono/Kernels/gcc_4.9/bin:$PATH
    SOURCE=~/kernel_build/chrono_kernel
    BUILD=~/kernel_build/obj_noselinux
    cd $SOURCE
    if [ "$1" == "clean" ] ; then
        make O=$BUILD mkproper
        make O=$BUILD clean
    fi
    if [ "$1" == "config" ] ; then
        make O=$BUILD codina_defconfig
    fi
    if [ "$1" == "menuconfig" ] ; then
        make O=$BUILD menuconfig
    fi
    
    make O=$BUILD \
    ARCH=arm \
    CROSS_COMPILE="/home/chrono/kernel_build/novatp_arm-eabi-4.9-master/bin/arm-eabi-" \
    -j5 -k
}
send() {
    adb push $BUILD/../$KERNEL_NAME /sdcard/$KERNEL_NAME
}
dump_config() {
    cp $BUILD/.config $SOURCE/arch/arm/configs/codina_defconfig
}
inst(){
    SOURCE=~/kernel_build/chrono_kernel
    BUILD=~/kernel_build/obj_noselinux
    rm $BUILD/../osfiles/osfiles_install.sh
    if [ "$1" == "codina" ] ; then
        ln $BUILD/../osfiles_install_codina.sh $BUILD/../osfiles/osfiles_install.sh
    fi
    
    if [ "$1" == "codinap" ] ; then
        ln $BUILD/../osfiles_install_codinap.sh $BUILD/../osfiles/osfiles_install.sh
    fi
    cd $BUILD
    rm -fr $BUILD/../system/lib/modules/
    mkdir $BUILD/../system/lib/modules/
    mkdir $BUILD/../system/lib/modules/autoload/
    make -C $SOURCE O=$BUILD modules_install INSTALL_MOD_PATH=$BUILD/../system/
    rm -f $BUILD/../system/lib/modules/ecryptfs.ko
    cp -f $BUILD/../system/lib/modules/param.ko $BUILD/../ramdisk/modules/param.ko
    cp -f $BUILD/../system/lib/modules/j4fs.ko $BUILD/../ramdisk/modules/j4fs.ko
    mv -f $BUILD/../system/lib/modules/bfq-iosched.ko $BUILD/../ramdisk/modules/autoload/bfq-iosched.ko
    mv -f $BUILD/../system/lib/modules/cpufreq_interactive.ko 
$BUILD/../ramdisk/modules/autoload/cpufreq_interactive.ko
    #mv -f $BUILD/../system/lib/modules/cpufreq_dynamic.ko $BUILD/../ramdisk/modules/autoload/cpufreq_dynamic.ko
    mv -f $BUILD/../system/lib/modules/cpufreq_zenx.ko $BUILD/../ramdisk/modules/autoload/cpufreq_zenx.ko
    mv -f $BUILD/../system/lib/modules/zcache.ko $BUILD/../ramdisk/modules/autoload/zcache.ko
    mv -f $BUILD/../system/lib/modules/cpufreq_ondemandplus.ko 
$BUILD/../ramdisk/modules/autoload/cpufreq_ondemandplus.ko
    mv -f $BUILD/../system/lib/modules/logger.ko $BUILD/../system/lib/modules/autoload/logger.ko
    cp -f arch/arm/boot/zImage ../boot.img
    if [ "$1" == "codina" ] ; then
        mkdir $BUILD/../codina
        KERNEL_NAME=codina/chrono_kernel_r$VERSION.zip
    fi
    
    if [ "$1" == "codinap" ] ; then
        mkdir $BUILD/../codinap
        KERNEL_NAME=codinap/chrono_kernel_r$VERSION.zip
    fi
    cd ..
    rm $KERNEL_NAME
    zip -r -9 $KERNEL_NAME META-INF system ramdisk genfstab osfiles recovery boot.img tmp
    cd $SOURCE
    #send
    #sudo shutdown -h now
}

upload() {
    if [ "$1" == "codinap" ] ; then
        $BUILD/../../uploader.py $PASS $BUILD/../$KERNEL_NAME /XDA-files/ChronoMonochrome/kernel/codinap
    fi
    
    if [ "$1" == "codina" ] ; then
        if [ "$2" == "debug" ] ; then
            $BUILD/../../uploader.py $PASS $BUILD/../$KERNEL_NAME /XDA-files/ChronoMonochrome/misc
        else
            $BUILD/../../uploader.py $PASS $BUILD/../$KERNEL_NAME /XDA-files/ChronoMonochrome/kernel/codina  
        fi
    fi
    
}

compile config
inst codina
inst codinap
upload codina
upload codinap
