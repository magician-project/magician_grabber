import numpy as np
import matplotlib
import matplotlib.pyplot as plt
#matplotlib.use('TkAgg')  # or 'Qt5Agg' if you have PyQt5 or PySide2
print(matplotlib.get_backend())
from SharedMemoryManager import SharedMemoryManager

def main(streamName): 

    WINDOW = 4000
 
    #=====================================================
    # Set up interactive plotting
    #=====================================================
    plt.ion()
    fig, axs = plt.subplots(2, 1, figsize=(10, 6))
    line1, = axs[0].plot([], [], label="Acceleration PSD")
    line2, = axs[1].plot([], [], label="Acceleration Spikeness")

    axs[0].set_title("Acceleration PSD")
    axs[1].set_title("Acceleration Spikeness")
    for ax in axs:
        ax.set_xlim(0, WINDOW)
        ax.set_ylim(-1, 1)
        ax.grid(True)
        ax.legend()
    plt.show(block=False)
    #=====================================================


    smm = SharedMemoryManager("libSharedMemoryVideoBuffers.so", 
                              descriptor = "tactile_frames.shm", 
                              frameName  = streamName,
                              connect    = True)

    # Loop to continuously read frames 
    while True:
        # Capture frame-by-frame
        frameRaw = smm.read_from_shared_memory()

        frame = np.frombuffer(frameRaw, dtype=np.float32)
        #Frame is bytes it should be cast as float32

        """
         =====================================================
         acceleration_psd.csv  2 values per measurement
         acceleration_spikeness.csv 2 values per measurement
         accelerometer.csv  4 values per measurement
         force.csv 2 values per measurement
         =====================================================
         force_psd.csv  4 values per measurement
         friction.csv 2 values per measurement
         =====================================================
         WINDOW = 4000
        """

        #=====================================================================
        frameStart = 0
        frameEnd   = WINDOW*2
        friction = frame[frameStart:frameEnd].reshape(-1, 2)
        print("friction shape:", friction.shape)
        print("friction :",friction)
        #=====================================================================
        frameStart = frameEnd
        frameEnd   = frameStart + WINDOW*2
        acceleration_spikeness = frame[frameStart:frameEnd].reshape(-1, 2)
        print("acceleration_spikeness shape:", acceleration_spikeness.shape) 
        print("acceleration_spikeness :",acceleration_spikeness)
        #=====================================================================
        frameStart = frameEnd
        frameEnd   = frameStart + WINDOW*2
        acceleration_psd = frame[frameStart:frameEnd].reshape(-1, 2)
        print("acceleration_psd shape:", acceleration_psd.shape) 
        print("acceleration_psd :", acceleration_psd)
        #=====================================================================
        frameStart = frameEnd
        frameEnd   = frameStart + WINDOW*2
        force_psd = frame[frameStart:frameEnd].reshape(-1, 2)
        print("force_psd shape:", force_psd.shape) 
        print("force_psd :", force_psd) 
        #=====================================================================

        print("Data :",frame)


        # Plot the first channel of each for simplicity
        #=====================================================
        line1.set_xdata(acceleration_psd[:, 0])
        line1.set_ydata(acceleration_psd[:, 1])

        line2.set_xdata(acceleration_spikeness[:, 0])
        line2.set_ydata(acceleration_spikeness[:, 1])

        for ax in axs:
            ax.relim()
            ax.autoscale_view()

        fig.canvas.draw()
        fig.canvas.flush_events()
        plt.pause(0.01)
        #=====================================================


if __name__ == "__main__":
    import sys
    streamName = "stream_tactile"
    if len(sys.argv) != 2 :
        print("\n\nYou did not supply a stream name, assuming ",streamName) 
    else:
        streamName = sys.argv[1]

    main(streamName)

