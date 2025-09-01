# Magician Unified Data Grabber

<img src="https://github.com/magician-project/magician_grabber/blob/main/doc/logo.jpg?raw=true" width=300/>
<img src="https://github.com/magician-project/magician_grabber/blob/main/doc/grabber.png?raw=true" width=300/>



## Compilation / Dependencies:

Building the Grabber and getting its dependencies is very easy, just issue:
```
scripts/build.sh
```

To recompile the binaries after changing the source code:
```
make
```


The most complex dependency of the grabber is [ARAVIS](https://github.com/AravisProject/aravis) for the GiGE camera.

To manually install it (if you dont want to use the [build.sh](https://github.com/magician-project/magician_grabber/blob/main/scripts/build.sh#L57)) you can do the following:



```
git clone https://github.com/AravisProject/aravis
cd aravis 
meson build
cd build
ninja
sudo ninja install
```


## Using Magician Grabber with or without ROS:


After running the scripts/build.sh script mentioned above that gets all dependencies and building ros-free binaries the grabber can be also compiled with ROS2 support.
To do so the magician_grabber folder should be placed on the ROS2 workspace and the grabber can be compiled regularly along the other ROS packages using colcon build!

Instead of using the magician_grabber or magician_grabber_tactile binaries that are not linked to ROS2, if you want ROS2 support you can just execute the rclcpp_magician_grabber binary
supplying it with the same command line parameters as you would to the other binaries. It should work exactly the same way as the standalone magician_grabber binaries, but also include
the broadcast of the required ROS topics and messages [as seen here](https://github.com/magician-project/magician_grabber/blob/main/ros_magician_grabber.cpp#L122).



Command	Description:

| Binary                      | Compile Using       | Description |
|-----------------------------|---------------------|-----------------------|
| magician_grabber            | make                | Grabber  |
| magician_grabber_tactile    | make                | Grabber + real-time computation of [tactile features](https://github.com/magician-project/magician_grabber/tree/main/tactile_processor) |
| rclcpp_magician_grabber     | colcon              | Grabber + [ROS2 publishing](https://github.com/magician-project/magician_grabber/blob/main/ros_magician_grabber.cpp#L122) |


## Usage:

The full list of commands can be given by executing 
```
./magician_grabber --help
```
or by looking at the [source code](https://github.com/magician-project/magician_grabber/blob/main/common.h#L312)

Of course instead of magician_grabber or magician_grabber_tactile binaries you can run the colcon generated rclcpp_magician_grabber binary depending on your needs.


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





