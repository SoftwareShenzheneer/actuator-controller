# actuator-controller
esp32s3 based actuator controller. It receives information from the actuator driver and provides this on a display using LVGL.

## On Windows

1. Activate esp environment
```
Windows + IDF_v6.1_Powershell
```

2. Navigate to build/ dir
```
cd "\\wsl.localhost\Ubuntu-22.04\home\tomi\projects\actuator-driver\build"
```

3. run flash command
```
python -m esptool --chip esp32s3 -p COM6 -b 460800 write-flash '@flash_args'
```

## On Ubuntu WSL
1. Activate esp environment
```
source ~/.espressif/tools/activate_idf_v6.1.sh
```

2. Build commands
```
fullclean
idf.py fullclean

build
idf.py build
```

