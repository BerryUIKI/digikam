#!/bin/bash

# SPDX-FileCopyrightText: 2008-2025 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Script to compile and install clang for clazy static analyzer

CLANG_SRC_DIR=./clang_clazy
CLANG_BUILD_DIR=./clang_clazy_build     # Important that this is outside of the source directory
CLANG_INSTALL_DIR=/opt/clang
CLANG_VERSION=llvmorg-19.1.7
CLANG_SHARED_LIB=$CLANG_INSTALL_DIR/lib/libclang.so

if [ ! -d $CLANG_SRC_DIR ] ; then

    git clone https://github.com/llvm/llvm-project.git -b $CLANG_VERSION $CLANG_SRC_DIR

fi

if [ -d "build" ]; then

    rm -fr $CLANG_BUILD_DIR

fi

mkdir $CLANG_BUILD_DIR
cd $CLANG_BUILD_DIR

cmake -DCMAKE_INSTALL_PREFIX=$CLANG_INSTALL_DIR/llvm -DLLVM_EXPORT_SYMBOLS_FOR_PLUGINS=ON -DLLVM_INCLUDE_EXAMPLES=OFF -DLLVM_TARGETS_TO_BUILD="X86" -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" -DCMAKE_BUILD_TYPE=Release -G "Ninja" ../$CLANG_SRC_DIR/llvm
cmake --build . --parallel

if [ -f $CLANG_SHARED_LIB ] ; then

    file $CLANG_SHARED_LIB

else

    echo "ERROR: $CLANG_SHARED_LIB do not exists!"

    exit -1

fi

sudo cmake --build . --target install

echo "NOTE: Clang is installed. Add /$CLANG_INSTALL_DIR/llvm/bin/ to the PATH env. variable..."

exit 0