unpack(){
    if ! [ -f cache/ncurses-6.2.tar.gz ]; then # Check if tarball exists
        curl -Lo cache/ncurses-6.2.tar.gz "https://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.2.tar.gz"
    fi

 	tar -xzf cache/ncurses-6.2.tar.gz
 	export BUILD_DIR=ncurses-6.2
}
 
buildp(){
 	cd $BUILD_DIR
 	patch -p1 < ../lemon-ncurses-6.2.patch
 	./configure --prefix=$LEMON_PREFIX --host=x86_64-lemon --enable-pc-files --without-ada --without-shared --without-cxx-shared \
        --with-termlib --enable-widec --enable-ext-colors --enable-sigwinch
	ln -sf $LEMON_SYSROOT/system/include/ncurses/* $LEMON_SYSROOT/system/include
 	make -j$JOBCOUNT
 	make install DESTDIR=$LEMON_SYSROOT
}
