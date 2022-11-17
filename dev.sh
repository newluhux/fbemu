#!/bin/sh

POS=$(dirname $(realpath $0))
guix shell -m $POS/dev-pkg.scm
