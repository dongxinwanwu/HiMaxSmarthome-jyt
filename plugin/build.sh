#! /bin/bash

RecursiveCompilation() {
  echo "build all plugins complete copy to ../build/smarthome/plugin"

  for file in $(ls $1); do
    if test -d $file; then
      cd $file  && ./build.sh && cd ..
    fi
  done
}

RecursiveCompilation ./
