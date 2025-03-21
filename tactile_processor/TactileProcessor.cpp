/** @file TactileProcessor.cpp
 *  @brief  A real-time feature calculation utility for the ATI/Accelerometer sensor

 *  @author Michele Pompilio
 */


#include "TactileFeaturesProcessor.hpp"


#define F_input_fs 7000
#define F_dec_fact 7
#define A_input_fs 4000
#define A_dec_fact 1

#define Fr_win_size 128 * F_dec_fact
#define Fr_ovlp 0.5
#define Fr_highcutf 450.0
#define Fr_lowcutf 10.0

#define As_win_size 2000 * A_dec_fact
#define As_ovlp 0.5
#define As_highcutf 100.0

#define Fpsd_win_size 5 * F_dec_fact
#define Fpsd_ovlp 0.8
#define Fpsd_lowcutf 5.0

#define Apsd_win_size 20 * A_dec_fact
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
  return 1;
}


int tactile_add_acc(unsigned long timestamp, double accX , double accY, double accZ)
{
  countAcc +=1 ;
  //std::cerr<<"tactile_add_acc with "<<accX<<" "<<accY<<"  "<<accZ<<"\n";
  double timestampF = ( double )  timestamp / 1000000;
  processor.addAccelerationData({timestampF, {accX, accY, accZ}});
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




#if TACTILE_LIBRARY
int tactile_main()
#else
int main (int argc, char **argv)
#endif
{
    std::string F_input_file = "ForceRaw.txt";
    std::ifstream infile(F_input_file);

    std::string A_input_file = "AccRaw.txt";
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

    /*while (processor.isProcessing())
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
    }*/

    infile.close();
    infile1.close();
    std::cout << "Processing complete." << std::endl;

    return 0;
}
