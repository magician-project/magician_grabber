# Magician Unified Data Grabber

<img src="https://github.com/magician-project/magician_grabber/blob/main/doc/logo.jpg?raw=true" width=300/>
<img src="https://github.com/magician-project/magician_grabber/blob/main/doc/grabber.png?raw=true" width=300/>


## Usage:

Using the Grabber is very easy:
```
./magician_grabber --camera --force --accelerometer --output datasetName  --time seconds 
```

Using the Grabber to capture everything using 5500 microseconds exposure time in camera:
```
./magician_grabber --all --output datasetName --exposure 5500 --time seconds 
```

To add tactile feature processing use the magician_grabber_tactile binary:
```
./magician_grabber_tactile --camera --force --accelerometer --output datasetName  --time seconds 
```

To stream tactile data to shared memory:
```
./magician_grabber_tactile --stream --accelerometer --force --nocamera --noarduino
```


## Compilation:

Building the Grabber is very easy, just issue:
```
scripts/build.sh
```

To recompile the binaries after changing the source code:
```
make
```


Its only dependency is ARAVIS
https://github.com/AravisProject/aravis


```
git clone https://github.com/AravisProject/aravis
cd aravis 
meson build
cd build
ninja
sudo ninja install
```




