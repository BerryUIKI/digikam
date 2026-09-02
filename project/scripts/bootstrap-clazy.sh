#!/bin/bash

# SPDX-FileCopyrightText: 2008-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Copy this script on root folder where are source code

#export VERBOSE=1

if [ ! -d "build" ]; then
    mkdir build
fi

cd build

CLANG_INSTALL_DIR=/opt/clazy
CLANG_SHARED_LIB=$CLANG_INSTALL_DIR/lib/libclang.so

#export Options='-DCLAZY_AST_MATCHERS_CRASH_WORKAROUND=ON \

export PATH=$CLANG_INSTALL_DIR/bin/:$PATH

/opt/qt6/bin/cmake -G "Ninja" \
      -DCMAKE_INSTALL_PREFIX=$CLANG_INSTALL_DIR \
      -DCLANG_LIBRARY_IMPORT=$CLANG_SHARED_LIB \
      -DCMAKE_BUILD_TYPE=Release \
      -DLINK_CLAZY_TO_LLVM=ON \
      -DAPPIMAGE_HACK=OFF \
      -DCLAZY_MAN_PAGE=OFF \
      -Wno-dev \
      ..

/opt/qt6/bin/cmake --build . --parallel
sudo /opt/qt6/bin/cmake --build . --target install
