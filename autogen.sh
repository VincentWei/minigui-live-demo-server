#!/bin/sh

cat m4/*.m4 > acinclude.m4
autoheader
aclocal
automake --foreign --add-missing --copy
autoconf
