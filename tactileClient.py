import numpy as np
from SharedMemoryManager import SharedMemoryManager

def main(streamName):
    
    smm = SharedMemoryManager("libSharedMemoryVideoBuffers.so", 
                              descriptor = "tactile_frames.shm", 
                              frameName  = streamName,
                              connect    = True)

    # Loop to continuously read frames 
    while True:
        # Capture frame-by-frame
        frame = smm.read_from_shared_memory()
        """
         acceleration_psd.csv  2 values per measurement
         acceleration_spikeness.csv 2 values per measurement
         accelerometer.csv  4 values per measurement
         force.csv 2 values per measurement
         force_psd.csv  4 values per measurement
         friction.csv 2 values per measurement

         WINDOW = 4000
        """
        WINDOW = 4000

        frameEnd = WINDOW*2
        frameStart = 0
        acceleration_psd = frame[frameStart:frameEnd] 

        frameStart = frameEnd
        frameEnd = frameStart + WINDOW*2
        acceleration_spikeness = frame[frameStart: frameEnd] 

        print("Data :",frame)
     

if __name__ == "__main__":
    import sys
    streamName = "stream1"
    if len(sys.argv) != 2 :
        print("\n\nYou did not supply a stream name, assuming ",streamName) 
    else:
        streamName = sys.argv[1]

    main(streamName)

