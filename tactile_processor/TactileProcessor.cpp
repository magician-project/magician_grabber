/** @file TactileProcessor.cpp
 *  @brief  A real-time feature calculation utility for the ATI/Accelerometer sensor

 *  @author Michele Pompilio
 */


#include "TactileFeaturesProcessor.hpp"


#define F_input_fs 7000
#define F_dec_fact 7
#define A_input_fs 4000
#define A_dec_fact 1

#define Fr_win_size 128
#define Fr_ovlp 0.5
#define Fr_highcutf 450.0
#define Fr_lowcutf 10.0

#define As_win_size 2000
#define As_ovlp 0.5
#define As_highcutf 100.0

#define Fpsd_win_size 128
#define Fpsd_ovlp 0.8
#define Fpsd_lowcutf 5.0

#define Apsd_win_size 128
#define Apsd_ovlp 0.8
#define Apsd_lowcutf 5.0


#if TACTILE_LIBRARY
#warning "Compiling as Tactile Library"
const char * Fr_filename = "";
const char * As_filename = "";
const char * Fpsd_filename = "";
const char * Apsd_filename = "";
#else
#define Fr_filename "Friction.txt"
#define As_filename "AccSpikeness.txt"
#define Fpsd_filename "Fpsd.txt"
#define Apsd_filename "Apsd.txt"
#endif // TACTILE_LIBRARY

unsigned long countAcc=0, countForce=0;
unsigned long countFr=0, countAs=0, countAps=0 , countFpsd=0;

//-------------------------------
std::vector<float> vfTimestamp;
std::vector<float> vfX;
std::vector<float> vfY;
std::vector<float> vfZ;
//-------------------------------
std::vector<float> vaccTimestamp;
std::vector<float> vaccX;
std::vector<float> vaccY;
std::vector<float> vaccZ;
//-------------------------------


//Main Processor 
TactileFeaturesProcessor processor(F_input_fs, F_dec_fact, A_input_fs, A_dec_fact,
                                   Fr_win_size, Fr_ovlp, Fr_highcutf, Fr_lowcutf, Fr_filename,
                                   As_win_size, As_ovlp, As_highcutf, As_filename,
                                   Fpsd_win_size, Fpsd_ovlp, Fpsd_lowcutf, Fpsd_filename,
                                   Apsd_win_size, Apsd_ovlp, Apsd_lowcutf, Apsd_filename);


int tactile_add_force(unsigned long timestamp, double fX , double fY, double fZ)
{
  countForce +=1 ;
  //std::cerr<<"tactile_add_force with "<<fX<<" "<<fY<<"  "<<fZ<<"\n";
  double timestampF = ( double )  timestamp / 1000000;
  processor.addForceData({timestampF, {fX, fY, fZ}});

  //Also hold raw data
  vfTimestamp.push_back(timestamp);
  vfX.push_back(fX);
  vfY.push_back(fY);
  vfZ.push_back(fZ);
  //--------------------

  return 1;
}


int tactile_add_acc(unsigned long timestamp, double accX , double accY, double accZ)
{
  countAcc +=1 ;
  //std::cerr<<"tactile_add_acc with "<<accX<<" "<<accY<<"  "<<accZ<<"\n";
  double timestampF = ( double )  timestamp / 1000000;
  processor.addAccelerationData({timestampF, {accX, accY, accZ}});


  //Also hold raw data
  vaccTimestamp.push_back(timestamp);
  vaccX.push_back(accX);
  vaccY.push_back(accY);
  vaccZ.push_back(accZ);
  //--------------------

  return 1;
}


int tactile_write_disk(FILE* FrFD, FILE* AsFD, FILE* ApsdFD , FILE* FpsdFD)
{
    int success = 0;

    countFr+=processor.Fr_processor.size();
    countAs+=processor.As_processor.size();
    countAps+=processor.Apsd_processor.size();
    countFpsd+=processor.Fpsd_processor.size();

    /*
    std::cerr<<"\n Acc Events "<<countAcc<<" \n";
    std::cerr<<"\n Force Events "<<countForce<<" \n";
    std::cerr<<"\n processor.Fr_processor.size() "<<countFr<<" \n";
    std::cerr<<"\n processor.As_processor.size() "<<countAs<<" \n";
    std::cerr<<"\n processor.Apsd_processor.size() "<<countAps<<" \n";
    std::cerr<<"\n processor.Fpsd_processor.size() "<<countFpsd<<" \n";*/

    if (processor.Fr_processor.size()>0)
    {
       std::vector<DataPoint> results = processor.getFrictionResults(processor.Fr_processor.size());

       for (unsigned int i =0; i<results.size(); i++)
       {
         fprintf(FrFD,"%f,%f\n",results[i].timestamp,results[i].values[0]);
       }
       success+=1;
    }

    if (processor.As_processor.size()>0)
    {
       std::vector<DataPoint> results = processor.getAccSpikeResults(processor.As_processor.size());

       for (unsigned int i =0; i<results.size(); i++)
       {
         fprintf(AsFD,"%f,%f\n",results[i].timestamp,results[i].values[0]);
       }
       success+=1;
    }

    if (processor.Apsd_processor.size()>0)
    {
       std::vector<DataPoint> results = processor.getAccPSDResults(processor.Apsd_processor.size());

       for (unsigned int i =0; i<results.size(); i++)
       {
         fprintf(ApsdFD,"%f,%f\n",results[i].timestamp,results[i].values[0]);
       }
       success+=1;
    }

    if (processor.Fpsd_processor.size()>0)
    {
       std::vector<DataPoint> results = processor.getForcePSDResults(processor.Fpsd_processor.size());

       for (unsigned int i =0; i<results.size(); i++)
       {
         fprintf(FpsdFD,"%f,%f\n",results[i].timestamp,results[i].values[0]);
       }
       success+=1;
    }

   return success==4;
}


int tactile_write_shared_memory(void* mem, unsigned int mem_size,unsigned int window_elements)
{
    float * memAsFloat = (float*) mem;

    int elements = (int) window_elements;
    if ( 
         (elements<=processor.Fr_processor.size()) &&
         (elements<=processor.As_processor.size()) &&
         (elements<=processor.Apsd_processor.size()) &&
         (elements<=processor.Fpsd_processor.size()) &&
         (elements<=vfTimestamp.size()) &&
         (elements<=vaccTimestamp.size()) 
       )
      {
        unsigned int memBase = 0;
        fprintf(stderr,"\n\nEmitting Tactile Shared Memory \n\n");
        //Friction
        //===============================================================================
        std::vector<DataPoint> resultsFr = processor.getFrictionResults(window_elements);
        for (unsigned int i =0; i<window_elements; i++)
         {
           memAsFloat[memBase + i*2 + 0] = (float) resultsFr[i].timestamp;
           memAsFloat[memBase + i*2 + 1] = (float) resultsFr[i].values[0];
         }
         memBase += window_elements * 2; 
        //===============================================================================

        //Acceleration spikes
        //===============================================================================
        std::vector<DataPoint> resultsAs = processor.getAccSpikeResults(window_elements);
        for (unsigned int i =0; i<window_elements; i++)
         {
           memAsFloat[memBase + i*2 + 0] = (float) resultsAs[i].timestamp;
           memAsFloat[memBase + i*2 + 1] = (float) resultsAs[i].values[0];
         }
         memBase += window_elements * 2; 
        //===============================================================================

        //Acceleration PSD
        //===============================================================================
        std::vector<DataPoint> resultsAccPSD = processor.getAccPSDResults(window_elements);
        for (unsigned int i =0; i<window_elements; i++)
         {
           memAsFloat[memBase + i*2 + 0] = (float) resultsAccPSD[i].timestamp;
           memAsFloat[memBase + i*2 + 1] = (float) resultsAccPSD[i].values[0];
         }
         memBase += window_elements * 2; 
        //===============================================================================

        //Force PSD
        //=============================================================================== 
        std::vector<DataPoint> resultsFPSD = processor.getForcePSDResults(window_elements);
        for (unsigned int i =0; i<window_elements; i++)
         {
           memAsFloat[memBase + i*2 + 0] = (float) resultsFPSD[i].timestamp;
           memAsFloat[memBase + i*2 + 1] = (float) resultsFPSD[i].values[0];
         }
         memBase += window_elements * 2; 
        //=============================================================================== 



        //fX fY fZ
        //===============================================================================
        for (unsigned int i=0; i<window_elements; i++)
         {
           memAsFloat[memBase + i*4 + 0] = (float) vfTimestamp[i];
           memAsFloat[memBase + i*4 + 1] = (float) vfX[i];
           memAsFloat[memBase + i*4 + 2] = (float) vfY[i];
           memAsFloat[memBase + i*4 + 3] = (float) vfZ[i];
         }

         vfTimestamp.erase(vfTimestamp.begin(), vfTimestamp.begin() + window_elements);
         vfX.erase(vfX.begin(), vfX.begin() + window_elements);
         vfY.erase(vfY.begin(), vfY.begin() + window_elements);
         vfZ.erase(vfZ.begin(), vfZ.begin() + window_elements);
         memBase += window_elements * 4; 
        //=============================================================================== 

        //accX accY accZ
        //===============================================================================
        for (unsigned int i=0; i<window_elements; i++)
         {
           memAsFloat[memBase + i*4 + 0] = (float) vaccTimestamp[i];
           memAsFloat[memBase + i*4 + 1] = (float) vaccX[i];
           memAsFloat[memBase + i*4 + 2] = (float) vaccY[i];
           memAsFloat[memBase + i*4 + 3] = (float) vaccZ[i];
         }

         vaccTimestamp.erase(vaccTimestamp.begin(), vaccTimestamp.begin() + window_elements);
         vaccX.erase(vaccX.begin(), vaccX.begin() + window_elements);
         vaccY.erase(vaccY.begin(), vaccY.begin() + window_elements);
         vaccZ.erase(vaccZ.begin(), vaccZ.begin() + window_elements);
         memBase += window_elements * 4; 
        //=============================================================================== 

 
        return 1;
      } 
      //else { fprintf(stderr,"\n\nNOT ENOUGH DATA FOR Emitting Tactile Shared Memory \n\n"); }
    return 0;
}


#if TACTILE_LIBRARY
int tactile_main()
#else
int main (int argc, char **argv)
#endif
{
    std::string F_input_file = "input.txt";
    std::ifstream infile(F_input_file);

    std::string A_input_file = "input1.txt";
    std::ifstream infile1(A_input_file);

    double timestampF, fx, fy, fz, flagF, timestampA, ax, ay, az, flagA;

    std::string line;
    while (std::getline(infile, line))
    {
        //std::cout << ".";

        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream iss(line);
        if (!(iss >> fx >> fy >> fz >> timestampF >> flagF))
        {
            break;
        }
        processor.addForceData({timestampF, {fx, fy, fz}});
    }


    while (std::getline(infile1, line))
    {
        //std::cout << "*";

        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream iss(line);
        if (!(iss >> ax >> ay >> az >> timestampA >> flagA))
        {
            break;
        }
        processor.addAccelerationData({timestampA, {ax, ay, az}});
    }

    while (processor.isProcessing())
    {
        //std::cout << "?";
        std::vector<DataPoint> results = processor.getFrictionResults(1);
        for (const auto& result : results)
        {
            std::cout << result.timestamp << " ";
            for (const auto& value : result.values)
            {
                std::cout << value << " ";
            }
            std::cout << std::endl;
        }
    }

    //std::cout << "Done";

    infile.close();
    infile1.close();
    std::cout << "Processing complete." << std::endl;

    return 0;
}
