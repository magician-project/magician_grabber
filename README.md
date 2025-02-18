# magician_grabber


## Usage:

Using the Grabber is very easy:
```
./magician_grabber --camera --force --accelerometer --output datasetName  

```



## Compilation:

Building the Grabber is very easy, just issue:
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




