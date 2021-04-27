#!/bin/sh

out=""

dotest(){
  if [ $(command -v "$1" ) ]; then
    out="$out $2"
  fi
}

dotest bat "-DHAS_BAT"
dotest affine "-DHAS_AFFINE"
dotest vex "-DHAS_VEX"

printf "$out"
