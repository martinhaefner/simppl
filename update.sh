#!/bin/bash

TARGET=$1

if [ "$TARGET" = distclean ]; then
   rm -rf build
   exit 0
fi

if [ ! -d build ]; then
   mkdir -p build || exit 1
fi

VAR_GTEST_ROOT=$GTEST

if [ -z "$VAR_GTEST_ROOT" ]; then
   VAR_GTEST_ROOT=$GTEST_ROOT
fi

if [ -z "$VAR_GTEST_ROOT" ]; then
   while read DIR; do
      VAR_GTEST_ROOT=$DIR
      break
   done < <(find ~ -maxdepth 1 -name gtest_build)
fi

pushd build

cmake .. -DGTEST_ROOT=$VAR_GTEST_ROOT || exit 1
make $TARGET || exit 1

popd

exit 0
