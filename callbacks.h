#ifndef CALLBACKS_H_INCLUDED
#define CALLBACKS_H_INCLUDED


#ifdef __cplusplus
extern "C"
{
#endif


//These are examples of callbacks
static int camera_callback(GiGECameraConfig *config, unsigned long timestamp, struct Image *dataAsImage)
{
    (void)config; (void)timestamp; (void)dataAsImage;
    return 0;
}

static int force_callback(ATINetFTConfig *atinetft_config,unsigned long timestamp,double fX,double fY,double fZ,double tX,double tY,double tZ)
{
    (void)timestamp;
    fprintf(stderr,"ATI callback for %s received %f %f %f %f %f %f\n",atinetft_config->csv_name,fX,fY,fZ,tX,tY,tZ);
    return 0;
}


static int accelerometer_callback(ArduinoSerialConfig *arduino_config, unsigned long timestamp, unsigned long dev_timestamp, double accX, double accY, double accZ)
{
    (void)timestamp; (void)dev_timestamp;
    fprintf(stderr,"Accelerometer callback for %s received %f %f %f\n",arduino_config->csv_name,accX,accY,accZ);
    return 0;
}

static int controller_callback(ArduinoSerialConfig *arduino_config,unsigned long timestamp, int button1,  int button2,
                               int D1,int D2, int D3,
                               int Light1,int Light2,int Light3,int Light4,int Light5,int Light6)
{
    (void)timestamp;
    fprintf(stderr,"Controller callback for %s received \n",arduino_config->csv_name);
    fprintf(stderr,"Buttons: %d %d \n",button1,button2);
    fprintf(stderr,"Distances: %d %d %d \n",D1,D2,D3);
    fprintf(stderr,"Lights: %d %d %d %d %d %d \n",Light1,Light2,Light3,Light4,Light5,Light6);
    return 0;
}

#ifdef __cplusplus
}
#endif


#endif // CALLBACKS_H_INCLUDED
