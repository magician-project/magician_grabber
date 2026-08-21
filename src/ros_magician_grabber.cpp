//To compile me :
//            colcon build
//To run me :
//             build/rclcpp_magician_grabber/magician_grabber 


//ROS Includes
#include <rclcpp/rclcpp.hpp>
//#include <rclcpp/rclcpp/rclcpp.hpp>
#include <std_msgs/msg/float32.hpp>
#include <geometry_msgs/msg/wrench_stamped.hpp>
#include <geometry_msgs/msg/accel_stamped.hpp>

//Regular imports
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

//Our modules
#include "common.h"
#include "arduinoSensor.h"
#include "atiForceSensor.h"
#include "gigeCameraSensor.h"
#include "resolveUSBDevice.h"
#include "tactileFeatures.h"
#include "callbacks.h"

//Shared memory for streaming
#include "imageStreamer.h"
#include "sharedMemoryVideoBuffers.h"

#include "performance.h"

static const char MagicianROSGrabberVersion[]="0.0.1";

volatile sig_atomic_t stop = 0;

void handle_sigint(int sig)
{
    printf("\nCaught signal %d (Ctrl + C). Exiting gracefully (%d/3)...\n", sig, stop);
    stop += 1;

    if (stop>3)
    {
      printf("Killing process...\n");
      exit(0);
    }
}

/*
int setOutputDirectory(GlobalConfig *cfg, const char * outputDirectory)
{
    if (cfg==0) { return 0; }
    snprintf(cfg->outputDirectory,512,"%s",outputDirectory);

    char enabledFileOutput = (strcmp(cfg->outputDirectory,"/dev/null")!=0);
    if (!enabledFileOutput)
    {
        //If there is no file output we are done now..
        return 1;
    }


    if (strcmp(outputDirectory,"./")!=0)
    {
     char makedircmd[2048]={0};
     snprintf(makedircmd,1024,"mkdir -p %.512s",cfg->outputDirectory);

     int z = system(makedircmd);
                if (z==0)
                {
                    fprintf(stderr,"Output Path set to \"%s\" \n",cfg->outputDirectory);
                }
                else
                {
                    fprintf(stderr,RED "Failed setting output Path to \"%s\" \n" NORMAL,cfg->outputDirectory);
                }
    }
    return 1;
}*/

/*
int noOutputDirectory(GlobalConfig *cfg)
{
  if (cfg==0) { return 0; }

  setOutputDirectory(cfg,"/dev/null");
  return 1;
}*/

int process_keyboard_input(ArduinoSerialConfig * arduino_config,int key)
{
  (void)arduino_config;
  int processed = 0;
  switch (key)
  {
      case '0': break;
      case '1': break;
      case '2': break;
      case '3': break;
      case '4': break;
      case '5': break;
      case '6': break;
      case '+': break;
      case '-': break;
  };

 //Return if keystroke processed
 return processed;
}



//These is a callback that triggers the next light
static int camera_callback_next_light(GiGECameraConfig *config, unsigned long timestamp, struct Image *dataAsImage)
{
    (void)timestamp; (void)dataAsImage;
    if (config!=NULL)
    {
      if (config->global!=NULL)
      {
        ArduinoSerialConfig * arduino = (ArduinoSerialConfig *) config->global->arduino_cfg;
        if (arduino!=0)
         {
          return arduino_signalNewFrame(arduino);
         }
      }
    }
    return 0;
}


class MagicianGrabber : public rclcpp::Node
{
 public:
    MagicianGrabber() : Node("magician_grabber")
    {
        //ATI Force Sensor 
        //--------------------------------------------------------------------------
// geometry_msgs/msg/Wrench wrench 
        publisherfXYZ_ = this->create_publisher<geometry_msgs::msg::WrenchStamped>("magician_grabber/wrench_sensed", 1);

/*
        publisherfX_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/fX", 10);
        publisherfY_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/fY", 10);
        publisherfZ_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/fZ", 10);
        publishertX_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/tX", 10);
        publishertY_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/tY", 10);
        publishertZ_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/tZ", 10);
*/
        //Teensy Accelerometer 
        //--------------------------------------------------------------------------
        //https://docs.ros2.org/foxy/api/geometry_msgs/msg/AccelStamped.html

        publisheraccXYZ_ = this->create_publisher<geometry_msgs::msg::AccelStamped>("magician_grabber/accel_sensed", 10);
/*
        publisheraccX_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/accX", 10);
        publisheraccY_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/accY", 10);
        publisheraccZ_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/accZ", 10);
*/
        //Light Controller / Distance Sensors 
        //--------------------------------------------------------------------------
        publisherButton_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/button", 10);
        publisherD1_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/distance1", 10);
        publisherD2_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/distance2", 10);
        publisherD3_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/distance3", 10);
        publisherL1_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/light1", 10);
        publisherL2_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/light2", 10);
        publisherL3_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/light3", 10);
        publisherL4_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/light4", 10);
        publisherL5_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/light5", 10);
        publisherL6_ = this->create_publisher<std_msgs::msg::Float32>("magician_grabber/light6", 10);
        
    }

    void update_FT(float fX,float fY, float fZ , float tX , float tY, float tZ)
    {
        geometry_msgs::msg::WrenchStamped combined;
        combined.header.stamp = get_clock()->now();
        //combined.header.stamp = 0; //std::chrono.now();
        combined.header.frame_id="ft_sensing_frame";
        combined.wrench.force.x =fX;
        combined.wrench.force.y =fY;
        combined.wrench.force.z =fZ;
        combined.wrench.torque.x =tX;
        combined.wrench.torque.y =tY;
        combined.wrench.torque.z =tZ;
        publisherfXYZ_->publish(combined);

    }


/*
    void update_Forces(float fX, float fY, float fZ)
    {
        std_msgs::msg::Float32 msg1, msg2, msg3;
        msg1.data = fX;
        msg2.data = fY;
        msg3.data = fZ;

        publisherfX_->publish(msg1);
        publisherfY_->publish(msg2);
        publisherfZ_->publish(msg3);

        //RCLCPP_INFO(this->get_logger(), "Published Forces: %.2f, %.2f, %.2f", fX, fY, fZ);
    }

    void update_Torques(float tX, float tY, float tZ)
    {
        std_msgs::msg::Float32 msg1, msg2, msg3;
        msg1.data = tX;
        msg2.data = tY;
        msg3.data = tZ;

        publishertX_->publish(msg1);
        publishertY_->publish(msg2);
        publishertZ_->publish(msg3);

        //RCLCPP_INFO(this->get_logger(), "Published Torques: %.2f, %.2f, %.2f", tX, tY, tZ);
    }
*/

    void update_Accelerometer(float accX, float accY, float accZ)
    {
        geometry_msgs::msg::AccelStamped combined;
        combined.header.stamp = get_clock()->now();
        combined.accel.linear.x   = accX;
        combined.accel.linear.y   = accY;
        combined.accel.linear.z   = accZ;
        combined.accel.angular.x = 0.0;
        combined.accel.angular.y = 0.0;
        combined.accel.angular.z = 0.0;
 
        publisheraccXYZ_->publish(combined);

        /* 
        std_msgs::msg::Float32 msg1, msg2, msg3;
        msg1.data = accX;
        msg2.data = accY;
        msg3.data = accZ;

        publisheraccX_->publish(msg1);
        publisheraccY_->publish(msg2);
        publisheraccZ_->publish(msg3);*/

        //RCLCPP_INFO(this->get_logger(), "Published Accelerations: %.2f, %.2f, %.2f", accX, accY, accZ);
    }


    void update_Button(float button)
    {
        std_msgs::msg::Float32 msg1;
        msg1.data = button; 

        publisherButton_->publish(msg1);


        //RCLCPP_INFO(this->get_logger(), "Published Buttons: %.2f", button);
    }


    void update_Lights(float L1, float L2, float L3, float L4, float L5, float L6)
    {
        std_msgs::msg::Float32 msg1, msg2, msg3, msg4, msg5, msg6;
        msg1.data = L1;
        msg2.data = L2;
        msg3.data = L3;
        msg4.data = L4;
        msg5.data = L5;
        msg6.data = L6;

        publisherL1_->publish(msg1);
        publisherL2_->publish(msg2);
        publisherL3_->publish(msg3);
        publisherL4_->publish(msg4);
        publisherL5_->publish(msg5);
        publisherL6_->publish(msg6);

        //RCLCPP_INFO(this->get_logger(), "Published Light: %.2f, %.2f, %.2f", L1, L2, L3, L4, L5, L6);
    }

    void update_Distances(float D1, float D2, float D3)
    {
        std_msgs::msg::Float32 msg1, msg2, msg3;
        msg1.data = D1;
        msg2.data = D2;
        msg3.data = D3; 

        publisherD1_->publish(msg1);
        publisherD2_->publish(msg2);
        publisherD3_->publish(msg3);

        //RCLCPP_INFO(this->get_logger(), "Published Distances: %.2f, %.2f, %.2f", D1, D2, D3);
    }


private:
    
    rclcpp::Publisher<geometry_msgs::msg::WrenchStamped>::SharedPtr publisherfXYZ_;
    rclcpp::Publisher<geometry_msgs::msg::AccelStamped>::SharedPtr publisheraccXYZ_;

/*
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publisherfX_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publisherfY_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publisherfZ_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publishertX_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publishertY_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publishertZ_;

    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publisheraccX_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publisheraccY_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publisheraccZ_;*/

    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publisherButton_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publisherD1_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publisherD2_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publisherD3_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publisherL1_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publisherL2_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publisherL3_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publisherL4_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publisherL5_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publisherL6_;
};

// Global pointer to the node
std::shared_ptr<MagicianGrabber> global_node;

// C-style callback function
extern "C" int ros_force_callback(ATINetFTConfig *atinetft_config,double fX, double fY, double fZ, double tX, double tY, double tZ)
{
    (void)atinetft_config;
    if (global_node) 
    {
        //global_node->update_Forces(fX, fY, fZ);
        //global_node->update_Torques(tX, tY, tZ);
        global_node->update_FT(fX,fY,fZ,tX,tY,tZ);
        return 1;
    }
    return 0;
}

extern "C" int ros_accelerometer_callback(ArduinoSerialConfig *arduino_config, unsigned long timestamp, unsigned long dev_timestamp, double accX, double accY, double accZ)
{
    (void)arduino_config; (void)timestamp; (void)dev_timestamp;
    if (global_node) 
    {
        global_node->update_Accelerometer(accX, accY, accZ); 
        return 1;
    }
    return 0;
}

extern "C" int ros_controller_callback(ArduinoSerialConfig *arduino_config,unsigned long timestamp, int button1, int button2,
                               int D1,int D2, int D3,
                               int Light1,int Light2,int Light3,int Light4,int Light5,int Light6)
{
    (void)arduino_config; (void)timestamp; (void)button2;
    if (global_node) 
    {
        global_node->update_Button(button1); 
        global_node->update_Distances(D1, D2, D3); 
        global_node->update_Lights(Light1, Light2, Light3, Light4, Light5, Light6); 
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    global_node = std::make_shared<MagicianGrabber>();



    banner(MagicianROSGrabberVersion);

    // Handle Ctrl+C to stop recording gracefully
    signal(SIGINT, handle_sigint);


    // Grabber Configurations
    GlobalConfig cfg={};

    //Set defaults for arduino/teensy
    snprintf(cfg.arduinoPath,128,"%s","/dev/ttyUSB0");
    snprintf(cfg.teensyPath,128,"%s","/dev/ttyACM0");
    snprintf(cfg.cameraStreamName,128,"%s","stream1");
    snprintf(cfg.tactileStreamName,128,"%s","stream_tactile");

    // Global flag for termination
    cfg.keep_running = 1;
    cfg.run_forever  = 1;
    cfg.countdown    = 0;
    cfg.speak        = 0;
    cfg.viewer       = 0;

    #if TACTILE
    char calculateTactileFeatures    = 0;
    #endif // TACTILE

    //fprintf(stderr,"Will manually trigger light changes!\n");
    //By default try to manually trigger light
    cfg.manual_trigger_light = 1;

    // Modules available to use
    cfg.interceptKeyboard = 1;
    cfg.useRAM       = 0;
    cfg.useArduino   = 1;
    cfg.useTeensy    = 0;
    cfg.useCamera    = 1;
    cfg.useATIForce  = 0;
    cfg.streamData   = 1;
    snprintf(cfg.atiIP,128,"192.168.137.201");
    cfg.atiPort = 49152;

    #if TACTILE
    cfg.calculateTactileFeatures    = 0;
    #endif // TACTILE

    // Camera Default settings
    cfg.width      = 2448;
    cfg.height     = 2048;
    cfg.exposure   = 650; // 0 means no setting
    cfg.gain       = 0.0;
    cfg.blackLevel = 0.0;
    cfg.frameRate  = 20.0; //Each image is 4.5MB,
    //this framerate writes 45MB/sec to disk which is a sane value
    //use --ram to store data on a tmpfs/ for higher speeds
    //use --rt to elevate priority for higher speeds

     
    setOutputDirectory(&cfg, "./");
    //if (fileOutput==0) {  }
    noOutputDirectory(&cfg); //<- ROS Has no file output
    cfg.maxTimeToGrabForInSeconds = 0; 

    //=============================================================
    //  See common.h -> parse_arguments() or run with --help
    //                 for all available options
    //=============================================================
    parse_arguments(&cfg,argc,argv);
    //=============================================================
    //=============================================================


    // Arduino commands
    char arduinoUseRoundLight[]    = {"r\n"};
    //char arduinoUseDistanceLight[] = {"a\n"};        // Not used currently
    char * arduinoExtraCommand = arduinoUseRoundLight; //0 Or Always set round lights on




   if (cfg.useRAM)
   {
       snprintf(cfg.outputDirectoryOriginal,1024,"%s",cfg.outputDirectory);
       int i = system("sudo mkdir tmpfs");
       if (i!=0)  { fprintf(stderr,RED "Failed creating a tmpfs directory to mount tmpfs \n" NORMAL); }

       i = system("sudo mount -t tmpfs -o size=4G tmpfs tmpfs/");
       if (i!=0)  { fprintf(stderr,RED "Failed creating a tmpfs mount.. :(\n" NORMAL); return 1; }
       //snprintf(cfg.outputDirectory,1024,"%s","tmpfs/");
   }
  

    //Record time that acquisition started (this will be considered as timestamp 0 from now on)
    unsigned long acquisitionStartTime = GetTickCountMicroseconds();

    pthread_t gigecamera_tid, arduino_tid, teensy_tid, atinetft_tid;


    // Initialize Configurations
    //To debug aravis connection use : arv-camera-test-0.10  -d stream
    GiGECameraConfig camera_config     = {&cfg, "3205040", "camera.csv", cfg.width, cfg.height, cfg.exposure, cfg.gain, cfg.blackLevel, cfg.frameRate, 0, NULL, &cfg.keep_running,0 , 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL };
    ATINetFTConfig atinetft_config     = {&cfg, "192.168.137.201",  49152, "force.csv",  NULL, &cfg.keep_running,0 , 0, 0, 0.0, (void*) ros_force_callback};
    ArduinoSerialConfig teensy_config  = {&cfg, "/dev/ttyACM0",    "accelerometer.csv", 115200, NULL, &cfg.keep_running, 0, NULL , 0, 0, 0.0,(void*)  ros_accelerometer_callback};
    ArduinoSerialConfig arduino_config = {&cfg, "/dev/ttyUSB0",    "controller.csv",    115200, NULL, &cfg.keep_running, 0, arduinoExtraCommand, 0, 0, 0.0, (void*) ros_controller_callback};


    //Copy teensy/arduino port
    //snprintf(teensy_config.port_name,128,"%s",cfg.teensyPath);
    //snprintf(arduino_config.port_name,128,"%s",cfg.arduinoPath);
    fprintf(stderr,"Arduino : %s \n",arduino_config.port_name);

    //Copy ATI IP/port from cfg
    snprintf(atinetft_config.ip_address,128,"%s",cfg.atiIP);
    atinetft_config.port = cfg.atiPort;
    if (cfg.useATIForce)
    { fprintf(stderr,"ATI NetFT address : %s:%d\n",atinetft_config.ip_address,atinetft_config.port); }


    //Make arduino_cfg visible!
    cfg.arduino_cfg = (void*) &arduino_config;

    //Make each camera frame trigger next light!
    if (cfg.manual_trigger_light)
    {
     camera_config.callback = (void*) camera_callback_next_light;
    }

    //Try to make arduino wake up correctly
    //system("stty -F /dev/ttyACM0 115200 raw -echo");
    //system("stty -F /dev/ttyACM1 115200 raw -echo");

    #if TACTILE
    interceptKeyboard = 0; //Do not intercept keyboard until crashes are resolved
    calculateTactileFeatures = (cfg.useTeensy) && (cfg.useATIForce);
    pthread_t tactile_tid;
    struct TactileDataState  tactile_config = {&cfg, &cfg.keep_running, 0, 0};
    if (calculateTactileFeatures)
    {
        teensy_config.callback   = (void*) addTactileAccelerometerReading;//accelerometer_callback;
        atinetft_config.callback = (void*) addTactileForceReading;//force_callback;
        pthread_create(&tactile_tid,    NULL, tactile_thread,    &tactile_config);
    }
    #endif // TACTILE


    StreamingContext * streaming_context=0;

    if (cfg.useCamera)
    {
      fprintf(stderr,"Configuring camera exposure pins..\n");
      int i=system("arv-tool-0.10 control LineSelector=Line3 LineMode=Output LineSource=ExposureActive LineInverter=0");
      if (i!=0)
                           {
                               fprintf(stderr,"Failed setting Aravis Camera Exposure pins, halting to protect LED COBs\n");
                               abort();
                           }
    }

    if ( (cfg.streamData) && (cfg.useCamera) )
                         {
                           fprintf(stderr,"Starting stream..\n");
                           //We transport the raw sensor as 1 channel! (hence the 1 in next line)
                           streaming_context = startStream("video_frames.shm", cfg.cameraStreamName, cfg.width, cfg.height, 1);
                           camera_config.camera_shm_stream = (void*) streaming_context;
                           //fprintf(stderr,"Main Thread shm=%p\n",streaming_context);
                           //fprintf(stderr,"Main Thread #2 shm=%p\n",camera_config.camera_shm_stream);
                           if (camera_config.camera_shm_stream==NULL)
                           {
                               fprintf(stderr,"Failed to start streaming to shared memory!\n");
                               exit(1);
                           }

                           if (streaming_context->frame==NULL)
                           {
                               fprintf(stderr,"Failed to establish video frame!\n");
                               exit(1);
                           }
                         }

    //Arduino takes some time to powerup
    if (cfg.useArduino)  { pthread_create(&arduino_tid,    NULL, arduino_thread,    &arduino_config);  }
    if (cfg.useTeensy)   {
                      /*
                      char *teensy_port = find_teensy_port();
                      if (teensy_port) { fprintf(stderr,GREEN "Teensy found on: %s\n" NORMAL, teensy_port); } else
                                       { fprintf(stderr,RED "Teensy port not found\n" NORMAL); exit(1); }*/
                        pthread_create(&teensy_tid,    NULL, arduino_thread,    &teensy_config);
                     }

    // Start Threads
    if (cfg.useCamera)   {
                       pthread_create(&gigecamera_tid, NULL, gigecamera_thread, &camera_config);
                       fprintf(stderr,"Waiting for camera to wake up ..\n");

                       //This is the most complex loop to start
                       //This is a busy wait but since it is only for a
                       //few seconds only on the start and makes code easier
                       //it is justified :)
                       unsigned int timeCheck = 0;
                       while (!camera_config.running)
                       {
                         fprintf(stderr,".");
                         usleep(10000);
                         timeCheck+=1;

                         if (timeCheck>300)
                         {
                           fprintf(stderr,"\nCamera timed-out (%u ticks)..\n",timeCheck);
                           cfg.keep_running = 0; //<- this will make the program exit
                           break;
                         }
                       }

                       if (camera_config.running)
                          { fprintf(stderr,"\nCamera online (%u ticks)..\n",timeCheck); }

                      }

    if (cfg.useATIForce) { pthread_create(&atinetft_tid,   NULL, atinetft_thread,   &atinetft_config); }

    //Enable keystrokes to be received without blocking execution
    int key = 0;
    if (cfg.interceptKeyboard)
          { set_nonblocking_mode(); }

    unsigned long startTime = GetTickCountMicroseconds();
    unsigned long currentTime = startTime;
    printf("ROS Node started.\n");
    // Run until flag is cleared (placeholder for user signal handling)
    while ((cfg.keep_running) && (stop==0))
    {
        rclcpp::spin_some(global_node);
    
        // Simulate main loop work
        //usleep(1000);

        if (cfg.interceptKeyboard)
             { key = get_keystroke(); }

        if (key == 'q')
        {  // Stop when 'q' is pressed
          fprintf(stderr, "\nUser requested exit (pressed 'q')\n");
          cfg.keep_running = 0;
          break;
        } else
        {
          process_keyboard_input(&arduino_config,key);
        }

        if (stop)
        {  // Stop when Ctrl+C is received
           fprintf(stderr,"\nTerminating because of signal\n");
           cfg.keep_running = 0;
           break;
        }

        currentTime = GetTickCountMicroseconds();
        unsigned long runningTimeInSeconds = (currentTime - startTime) / 1000000;


        printf("\r");
        //-----------------------------------------------------------------------------------------------------------------

        if (cfg.streamData) { broadcasting(camera_config.framesCaptured); }
        if (cfg.run_forever)  { printf(GREEN " %lu sec " NORMAL, runningTimeInSeconds ); } else
                          {
                           printf(GREEN " %lu sec " NORMAL,cfg.maxTimeToGrabForInSeconds - runningTimeInSeconds );
                           progress_bar(runningTimeInSeconds,cfg.maxTimeToGrabForInSeconds);
                          }

        if (cfg.useCamera)
            {
             printf("|Cam %lu %0.2fHz ",camera_config.framesCaptured, camera_config.actualFrameRate);
             printf(" Ok %lu/Fail %lu/Under %lu",camera_config.n_completed_buffers, camera_config.n_failures,camera_config.n_underruns);
            }

        if (cfg.useArduino)  { printf("|Arduino %0.2fHz/%lu samples",arduino_config.Hz, arduino_config.receivedDataFrames ); }
        if (cfg.useTeensy)   { printf("|Teensy %0.2fHz/%lu samples",teensy_config.Hz, teensy_config.receivedDataFrames ); }
        if (cfg.useATIForce) { printf("|ATI %0.2fHz/%lu samples",atinetft_config.Hz, atinetft_config.receivedDataFrames); }
        //-----------------------------------------------------------------------------------------------------------------
        printf("|          \r");
        fflush(stdout);



        if ( (!cfg.run_forever) && (currentTime-startTime > cfg.maxTimeToGrabForInSeconds * 1000000) )
        {
          printf(GREEN "\n\n\n\nSuccesfully Completed recording time..\n" NORMAL);
          cfg.keep_running = 0;
          usleep(10000);
        }
    }

    //Restore terminal to its former state
    if (cfg.interceptKeyboard)
         { restore_terminal_mode(); }

    // Wait for threads to finish
    if (cfg.useTeensy)   { fprintf(stderr,"Releasing Teensy\n");  pthread_join(teensy_tid, NULL);     }
    if (cfg.useArduino)  { fprintf(stderr,"Releasing Arduino\n"); pthread_join(arduino_tid, NULL);    }
    if (cfg.useATIForce) { fprintf(stderr,"Releasing ATI\n");     pthread_join(atinetft_tid, NULL);   }

    printf("\n\n");
 
    //Record time that acquisition started (this will be considered as timestamp 0 from now on)
    unsigned long elapsedAcquisitionTime = GetTickCountMicroseconds() - acquisitionStartTime;
    printf("Data collection terminated after %0.2f seconds\n", (double) elapsedAcquisitionTime / 1000000.0);


    usleep(100000);
    


    rclcpp::shutdown();
    if (cfg.useCamera)   { fprintf(stderr,"Releasing Camera\n");  pthread_join(gigecamera_tid, NULL); }
    return 0;
}
