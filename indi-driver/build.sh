#!/bin/bash
mkdir build
cd build
rm * -rf
cmake -DCMAKE_INSTALL_PREFIX=/usr ../indi-ipfocuser/
sudo make install

