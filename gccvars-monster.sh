# filename: gccvars.sh  
# 'source gccvars.sh' to set the environment of gcc  
export C_INCLUDE_PATH=/usr/include/x86_64-linux-gnu:$C_INCLUDE_PATH  
export CPLUS_INCLUDE_PATH=$C_INCLUDE_PATH  
export OBJC_INCLUDE_PATH=$C_INCLUDE_PATH  
export LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LIBRARY_PATH  

export MKLROOT=/opt/intel/composerxe-2011.0.084/mkl
export GCCDIR=/home/harshas/compiler/gcc-cilk

export PATH=$GCCDIR/bin:$PATH  
export LD_LIBRARY_PATH=$MKLROOT/lib/intel64:$GCCDIR/lib:$GCCDIR/lib64:/usr/local/lib:$LD_LIBRARY_PATH 
export MANPATH=$GCCDIR/share/man:$MANPATH  