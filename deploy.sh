#!/bin/bash

rsync -zriPt --exclude=build --exclude=cmake-build* --exclude=cmake-cache --exclude=*.pcap $(pwd)/* scanzio@172.16.3.10:~/ns-3-dev | grep '^<' | awk '{ print $2 }'
# rsync -zriPt --exclude=build --exclude=cmake-build* --exclude=cmake-cache --exclude=*.pcap --exclude=*.dat $(pwd)/* scanzio@130.192.36.33:~/Desktop/ns-3-dev | grep '^<' | awk '{ print $2 }'