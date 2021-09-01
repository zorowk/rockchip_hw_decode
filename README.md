# This code is based on the v4l2 camera data capture  usage RGA

## Software requirements:
* Qt
* Cmake

## Example:
```
apt-get install -y git cmake libdrm-dev g++ librga-dev
cmake ./
make

// Adapt in rk3399
sudo -u user DISPLAY=:0 ./rga_v4l2
```

