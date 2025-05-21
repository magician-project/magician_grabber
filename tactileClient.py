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
        print("Data :",frame)
     

if __name__ == "__main__":
    import sys
    streamName = "stream1"
    if len(sys.argv) != 2 :
        print("\n\nYou did not supply a stream name, assuming ",streamName) 
    else:
        streamName = sys.argv[1]

    main(streamName)

