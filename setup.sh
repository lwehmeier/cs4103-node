#!/bin/bash
clear
echo Updating and initialising submodules where required
git submodule update --init

clear
echo Building syslog server...
cd tools/syslog-server
mvn package
cd ../..

clear
echo Preparing python venv..
mkdir -p tools/python-venv
# will fail in nested venvs. Known python bug for years..
python3 -m venv tools/python-venv
source tools/python-venv/bin/activate
cd tools/pssh
python setup.py build
python setup.py install
pip install python-igraph
