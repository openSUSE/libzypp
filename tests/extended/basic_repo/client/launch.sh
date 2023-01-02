#!/bin/bash

# the launch script is supposed to add the repository so that
# zypp-testrunner can refresh and install ptest from it.
# Do not refresh the repo here!

set -e
zypper -n ar http://172.16.238.10/basic basic
