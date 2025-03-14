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

static int accelerometer_callback(ArduinoSerialConfig *teensy_config,unsigned long timestamp,const char * line,unsigned int lineLength)
{
    fprintf(stderr,"\n\nTeensy callback for %s received %s\n",teensy_config->csv_name,line);

    unsigned long dev_timestamp=0;
    int accX=0, accY=0, accZ=0;

    // Parse the comma-separated values
    if (sscanf(line, "%lu,%d,%d,%d", &dev_timestamp, &accX, &accY, &accZ) == 4)
    {
        printf("Parsed values - Timestamp: %lu, AccX: %d, AccY: %d, AccZ: %d\n", dev_timestamp, accX, accY, accZ);
        return 1;  // Success
    } else
    {
        fprintf(stderr, "Error: Invalid format in line: %s\n", line);
        return 0; // Error in parsing
    }

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

static int controller_callback(ArduinoSerialConfig *arduino_config,unsigned long timestamp,const char * line,unsigned int lineLength)
{
    fprintf(stderr,"Arduino callback for %s received %s\n",arduino_config->csv_name,line);

    unsigned long dev_timestamp=0;

    int Button1;
    int Distance1;
    int Distance2;
    int Distance3;

    int Light1;
    int Light2;
    int Light3;
    int Light4;
    int Light5;
    int Light6;

    // Parse the comma-separated values
    if (sscanf(line, "%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &dev_timestamp, &Button1, &Distance1, &Distance2, &Distance3, &Light1, &Light2, &Light3, &Light4, &Light5, &Light6) == 11)
    {
        printf("Parsed values - Timestamp: %lu, Button1: %d, Distance1: %d, Distance2: %d, Distance3: %d\n", dev_timestamp, Button1, Distance1, Distance2, Distance3);
        return 1;  // Success
    } else
    {
        fprintf(stderr, "Error: Invalid format in line: %s\n", line);
        return 0; // Error in parsing
    }

    return 0;
}

#ifdef __cplusplus
}
#endif


#endif // CALLBACKS_H_INCLUDED
