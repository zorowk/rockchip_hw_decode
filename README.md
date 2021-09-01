# This code is based on the v4l2 camera data capture  usage RGA

## Software requirements:
* Qt
* Cmake

## Example:
```
apt-get install -y git cmake libdrm-dev g++ librga-dev
cmake ./
make

sudo -u firefly DISPLAY=:0 ./rkisp_demo -c 300 -d /dev/video0 -w 640 -h 480
or
sudo -u firefly DISPLAY=:0 ./rkisp_demo -c 300 -d /dev/video5 -w 640 -h 480
```
