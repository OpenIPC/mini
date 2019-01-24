#!/bin/bash
set -e

docker rm $(docker ps -a | grep Exit | cut -d ' ' -f 1) | exit 0
docker rmi $(docker images | tail -n +2 | awk '$1 == "<none>" {print $'3'}') | exit 0

rm -rf ./output/*
docker build -t openwrt_httpmini_demo -f $(pwd)/Dockerfile $(pwd) \
&& docker run -it -v $(pwd)/output/:/output/ openwrt_httpmini_demo


# copy files to/from camera

IP_ADDRESS=192.168.0.10

# scp -oUserKnownHostsFile=/dev/null -oStrictHostKeyChecking=no root@${IP_ADDRESS}:/tmp/stream_chn0.h264 ./output

scp -oUserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no $(pwd)/pub/index.html root@${IP_ADDRESS}:/tmp/
ssh -oUserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -q root@${IP_ADDRESS} mkdir -p /tmp/js
scp -oUserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no $(pwd)/pub/js/bundle.js root@${IP_ADDRESS}:/tmp/js/
scp -oUserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no $(pwd)/output/minihttp root@${IP_ADDRESS}:/tmp/
# ssh -oUserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -q root@${IP_ADDRESS} /tmp/minihttp
# scp -oUserKnownHostsFile=/dev/null -oStrictHostKeyChecking=no root@${IP_ADDRESS}:/tmp/svenc.jpg ./output
scp -oUserKnownHostsFile=/dev/null -oStrictHostKeyChecking=no root@${IP_ADDRESS}:/tmp/stream_chn0.h264 ./output
scp -oUserKnownHostsFile=/dev/null -oStrictHostKeyChecking=no root@${IP_ADDRESS}:/tmp/stream_chn0.mjp ./output

# ssh -oUserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no root@192.168.0.10
# cd /tmp && ./load3518e -a -sensor imx222
