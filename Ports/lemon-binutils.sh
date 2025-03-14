export BINUTILS_SRC_DIR=binutils-2.32

unpack(){
    wget "http://ftpmirror.gnu.org/binutils/binutils-2.32.tar.gz"
    tar -xzf binutils-2.32.tar.gz
 	rm binutils-2.32.tar.gz
}

buildp(){
	cd $BINUTILS_SRC_DIR
    patch -p1 < ../../Toolchain/lemon-binutils-2.32.patch
    cd ld
    aclocal
    automake
    autoreconf
    cd ..
    ac_cv_func_getrusage=no ac_cv_func_getrlimit=no ./configure --host=x86_64-lemon --target=x86_64-lemon --prefix=/system --disable-werror --enable-shared --disable-nls
    MAKEINFO=true make -j $JOBCOUNT all-binutils all-gas all-ld
    make install-binutils install-gas install-ld DESTDIR=$LEMON_SYSROOT
}
