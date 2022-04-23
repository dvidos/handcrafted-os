#!/bin/bash

echo "HOME is $HOME"

export PATH="$PATH:/usr/bin:$HOME/opt/cross-compiler/bin"
echo "PATH is $PATH"

make
