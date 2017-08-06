# filename: gccvars.sh  
# 'source gccvars.sh' to set the environment of gcc  
export C_INCLUDE_PATH=/usr/include/x86_64-linux-gnu:$C_INCLUDE_PATH  
export CPLUS_INCLUDE_PATH=$C_INCLUDE_PATH  
export OBJC_INCLUDE_PATH=$C_INCLUDE_PATH  
export LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LIBRARY_PATH  

#export GCCDIR=/opt/gcc-cilkplus-install  
export MKLROOT=/opt/intel/composer_xe_2013.3.163/mkl
export GCCDIR=/home/hsimhadr/gcc-cilk

export PATH=$GCCDIR/bin:$PATH  
export LD_LIBRARY_PATH=$MKLROOT/lib/intel64/:$GCCDIR/lib:$GCCDIR/lib64:/opt/gmp-5.0.5/lib:/opt/mpfr-3.1.1/lib:/opt/mpc-0.9/lib:$LD_LIBRARY_PATH  
export MANPATH=$GCCDIR/share/man:$MANPATH  

export CILK_NWORKERS=64