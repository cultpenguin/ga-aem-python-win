#!/bin/sh

#Set the directory path for dependencies
export srcdir='../src'
export cpputilssrc='../submodules/cpp-utils/src'
export tntdir='../submodules/tnt'
export geophysics_netcdf='/short/cr78/rcb547/code/repos/geophysics-netcdf'

#GNU compiler on raijin.nci.org.au
#module load ga-aem/dev-gnu
#export cxx=g++
#export mpicxx=mpiCC
#export cxxflags='-std=c++11 -O3 -Wall -fdiagnostics-color=always'
#export exedir='../bin/raijin/gnu'

#Intel compiler on raijin.nci.org.au
module load ga-aem/dev-intel
export cxx=icpc
export mpicxx=mpiCC
export cxxflags='-std=c++11 -O3 -Wall -diag-disable remark -D_GLIBCXX_USE_CXX11_ABI=0'
export exedir='../bin/raijin/intel'

module list
echo ---------------------------------------
echo cxx = $cxx
echo mpicxx = $mpicxx ... which is ...
$mpicxx -showme
echo ---------------------------------------


#Compiled as shared libs
make -f gatdaem1d_python.make $1
make -f gatdaem1d_matlab.make $1

#Compile without MPI
make -f ctlinedata2sgrid.make $1
make -f ctlinedata2slicegrids.make $1
make -f example_forward_model.make $1
make -f gaforwardmodeltdem.make $1

#Compile with MPI
make -f galeisbstdem.make $1
make -f garjmcmctdem.make $1
make -f galeiallatonce.make $1
make -f galeisbsfdem.make $1

#cd ..
#installpath='/short/public/rcb547/apps/ga-aem/dev'

#mkdir -p $installpath
#mkdir -p $installpath/bin/raijin/intel

#cp -pru docs $installpath
#cp -pru examples $installpath
#cp -pru python $installpath
#cp -pru matlab $installpath
#cp -pru bin/raijin/intel $installpath/bin/raijin

#chmod -R go+rx $installpath/bin/raijin/intel/*.exe
#chmod -R go+rx $installpath/matlab/bin/linux/*.so
#chmod -R go+rx $installpath/python/gatdaem1d/*.so


