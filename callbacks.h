#ifndef CALLBACKS_H_INCLUDED
#define CALLBACKS_H_INCLUDED


#ifdef __cplusplus
extern "C"
{
#endif


//These are examples of callbacks
static int camera_callback(GiGECameraConfig *camera_config)
{
    return 0;
}

static int force_callback(ATINetFTConfig *atinetft_config,unsigned long timestamp,double fX,double fY,double fZ,double tX,double tY,double tZ)
{
    fprintf(stderr,"ATI callback for %s received %f %f %f %f %f %f\n",atinetft_config->csv_name,fX,fY,fZ,tX,tY,tZ);
    return 0;
}



static int accelerometer_callback(ArduinoSerialConfig *arduino_config, unsigned long timestamp, unsigned long dev_timestamp, double accX, double accY, double accZ)
{
    fprintf(stderr,"Accelerometer callback for %s received %f %f %f %f %f %f\n",arduino_config->csv_name,accX,accY,accZ);
    return 0;
}


static int distance_callback(ArduinoSerialConfig *arduino_config,unsigned long timestamp,unsigned int D1,unsigned int D2,unsigned int D3)
{
    return 0;
}

static int button_callback(ArduinoSerialConfig *arduino_config,unsigned long timestamp,unsigned int B1)
{
    return 0;
}

static int controller_callback(ArduinoSerialConfig *arduino_config,unsigned long timestamp, int button,
                               int D1,int D2, int D3,
                               int Light1,int Light2,int Light3,int Light4,int Light5,int Light6)
{
    fprintf(stderr,"Controller callback for %s received \n",arduino_config->csv_name);
    fprintf(stderr,"Button: %d \n",button);
    fprintf(stderr,"Distances: %d %d %d \n",D1,D2,D3);
    fprintf(stderr,"Lights: %d %d %d %d %d %d \n",Light1,Light2,Light3,Light4,Light5,Light6);
}

#ifdef __cplusplus
}
#endif


#endif // CALLBACKS_H_INCLUDED
