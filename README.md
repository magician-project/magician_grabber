# Magician Unified Data Grabber

<img src="https://github.com/magician-project/magician_grabber/blob/main/doc/logo.jpg?raw=true" height=200/> <img src="https://github.com/magician-project/magician_grabber/blob/main/doc/grabber.png?raw=true" height=200/>



## Compilation / Dependencies:

Building the Grabber and getting its dependencies is [very easy](https://github.com/magician-project/magician_grabber/blob/main/scripts/build.sh), just issue:
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


## Using the Magician Grabber with or without ROS:


After running the scripts/build.sh script mentioned above that gets all dependencies and building the ros-free magician_grabber and magician_grabber_tactile binaries, the grabber can be also compiled including ROS2 support.
To do so the whole magician_grabber folder should be placed on the ROS2 workspace as a ROS package and the grabber can be compiled regularly along the other ROS packages using colcon build!

Instead of using the magician_grabber or magician_grabber_tactile binaries that are not linked to ROS2, if you want ROS2 support you must execute the rclcpp_magician_grabber binary.
It also needs to be supplied with the same command line parameters as you would to the other binaries. It should work exactly the same way as the standalone magician_grabber binaries, but also include
the broadcast of the required ROS topics and messages [as seen here](https://github.com/magician-project/magician_grabber/blob/main/ros_magician_grabber.cpp#L122).
The [default initialization of the ROS2 package](https://github.com/magician-project/magician_grabber/blob/main/ros_magician_grabber.cpp#L356) is a little different than the vanilla binaries to make it easier to invoke it



Command	Description:

| Binary                      | Compiled via        | Description |
|-----------------------------|---------------------|-----------------------|
| magician_grabber            | make                | Just the Grabber, very easy to debug!  |
| magician_grabber_tactile    | make                | Grabber + real-time computation of [tactile features](https://github.com/magician-project/magician_grabber/tree/main/tactile_processor) |
| rclcpp_magician_grabber     | colcon              | Grabber + [ROS2 publishing](https://github.com/magician-project/magician_grabber/blob/main/ros_magician_grabber.cpp#L122) |


## Usage:

Configuration of the grabber happens through command line parameters. 

E.g.:

```
./magician_grabber parameter1 parameter2 ... parameterN
```


The full list of accepted parameters is :

| Option                    | Description                                                     |
| ------------------------- | --------------------------------------------------------------- |
| `--simulate`              | Simulate devices (development).                                 |
| `-o, --output <path>`     | Set the output directory.                                       |
| `--arduino <path>`        | Set the path to Arduino (default: `/dev/ttyUSB0`).              |
| `--teensy <path>`         | Set the path to Teensy (default: `/dev/ttyACM0`).               |
| `--nooutput`              | Disable file output (redirect to `/dev/null`).                  |
| `--countdown <seconds>`   | Perform a countdown before starting.                            |
| `--view`                  | Use experimental Viewer.                                                     |
| `--ram`                   | Store data in RAM (recommended for high FPS).                   |
| `--trigger`               | Manually trigger light change after each captured frame.        |
| `--notrigger`             | Do not manually trigger light change after each captured frame. |
| `--size <width> <height>` | Set the camera resolution in pixels.                            |
| `--exposure <microsec>`   | Set camera exposure time in microseconds.                       |
| `--gain <value>`          | Set camera gain.                                                |
| `--fps <Hz>`              | Set camera frame rate (use `--ram` for FPS > 10).               |
| `--blacklevel <value>`    | Set camera black level.                                         |
| `--duration <seconds>`    | Set the maximum time for frame grabbing.                        |
| `--time <seconds>`        | Alias for `--duration`.                                         |
| `--forever`               | Run indefinitely.                                               |
| `--camera`                | Enable the camera.                                              |
| `--force`                 | Enable force sensor.                                            |
| `--features`              | Enable force sensor features calculation.                       |
| `--accelerometer`         | Enable accelerometer (Teensy device).                           |
| `--distance`              | Enable distance sensor (Arduino device).                        |
| `--dlight`                | Use lighting based on distance sensor.                          |
| `--rlight`                | Use round-robin lighting.                                       |
| `--tlight`                | Use patterned lighting.                                         |
| `--speak`                 | Enable TTS (text-to-speech).                                    |
| `--rt`                    | Set real-time priority (requires privileges).                   |
| `--all`                   | Enable all available devices.                                   |
| `--stream`                | Stream camera data to shared memory (disables file output).     |
| `--scan`                  | Scan using Arduino and exit.                                    |
| `--help`                  | Show this help message and exit.                                |
| `--silent`                | Suppress progress messages.                                     |
| `--unixtime`              | Use Unix time for timestamps.                                   |



This list of accepted commands can also be provided by executing:
```
./magician_grabber --help
``` 

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
./magician_grabber_tactile --stream --accelerometer --force --nocamera --noarduino --atiip 192.168.1.1
```


To stream tactile data (ATI/Teensy) to other ROS nodes:
```
build/rclcpp_magician_grabber/magician_grabber --stream --accelerometer --force --nocamera --noarduino --atiip 192.168.1.1
```





