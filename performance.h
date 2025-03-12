#ifndef PERFORMANCE_H_INCLUDED
#define PERFORMANCE_H_INCLUDED

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <pwd.h>
#include <sched.h>
/**
 * @brief Function to stick the thread that calls the function to a specific CPU core, if the core does not exist it will wrap-around depending on the number of cores.
 * @param Core we want to associate this thread with.
 * @return Returns 0 on success, see pthread_setaffinity_np return values for possible failure codes.
 */
static int stick_this_thread_to_core(int core_id)
{
   #if _GNU_SOURCE
   int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
   int core_id_mod_cores = core_id % num_cores;

   //if (core_id < 0 || core_id >= num_cores)
   //   return EINVAL;

   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(core_id_mod_cores, &cpuset);

   pthread_t current_thread = pthread_self();
   return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
   #else
    fprintf(stderr,"Cannot stick thread to core without GNU source extensions during compilation\n");
    return EFAULT;
   #endif
}



/**
 * @brief Function to change the priority of the process using `nice()`
 * @param priority The niceness value to set (-20 for highest priority, 19 for lowest)
 * @return Returns 0 on success, -1 on failure.
 */
static int set_process_nice(int priority)
{
    int ret = nice(priority);
    if (ret == -1 && errno != 0)  // Check for errors
    {
        perror("Failed to change process priority");
        return 0;
    }

    fprintf(stderr, "Process priority (nice value) set to %d\n", priority);
    return 1;
}



/**
 * @brief Function to increase the process priority using sudo and renice.
 * @param priority The nice value to set (-20 for highest priority, 19 for lowest)
 * @return Returns 0 on success, -1 on failure.
 */
static int elevate_nice_priority(int priority)
{
    pid_t pid = getpid();  // Get current process ID

    // Construct command: sudo renice -n <priority> -p <pid>
    char command[128];
    snprintf(command, sizeof(command), "sudo renice -n %d -p %d", priority, pid);

    fprintf(stderr, "Executing command: %s\n", command);

    // Execute the system command
    int ret = system(command);
    if (ret != 0)
    {
        fprintf(stderr, "Failed to elevate process priority\n");
        return -1;
    }

    fprintf(stderr, "Process priority successfully elevated to %d\n", priority);
    return 0;
}


/**
 * @brief Function for setting the real-time priority of a thread.
 * @return Returns 0 on success, -1 on failure.
 */
static int set_realtime_thread_priority()
{
    int ret;

    // We'll operate on the currently running thread.
    pthread_t this_thread = pthread_self();
    // struct sched_param is used to store the scheduling priority
    struct sched_param params;

    // We'll set the priority to the maximum.
    params.sched_priority = sched_get_priority_max(SCHED_FIFO);

    fprintf(stderr,"Trying to set thread realtime prio = %u \n",params.sched_priority);

    // Attempt to set thread real-time priority to the SCHED_FIFO policy
    ret = pthread_setschedparam(this_thread, SCHED_FIFO, &params);
    if (ret != 0)
    {
        // Print the error
        fprintf(stderr,"Failed setting thread realtime priority\n");
        return 0;
    }

    // Now verify the change in thread priority
    int policy = 0;
    ret = pthread_getschedparam(this_thread, &policy, &params);
    if (ret != 0)
    {
        fprintf(stderr,"Couldn't retrieve real-time scheduling paramers\n");
        return 0;
    }

    // Check the correct policy was applied
    if(policy != SCHED_FIFO)
    {
        fprintf(stderr,"Scheduling is NOT SCHED_FIFO!\n");
    }
    else
    {
        fprintf(stderr,"SCHED_FIFO OK\n");
    }

    // Print thread scheduling priority
    fprintf(stderr,"Thread priority is now %u\n",params.sched_priority);
    return 1;
}



/**
 * @brief Function to drop root privileges after setting priority.
 * @return Returns 0 on success, -1 on failure.
 */
static int drop_privileges(const char * user)
{
    if (user == 0)
    {
     // Drop group privileges first
     if (setgid(1000) != 0)
     {
        fprintf(stderr, "Failed to set GID to 1000\n");
        return 0;
     }

     // Drop user privileges
     if (setuid(1000) != 0)
     {
        fprintf(stderr, "Failed to set UID to 1000\n");
        return 0;
     }

    fprintf(stderr, "Privileges dropped successfully to UID=%d, GID=%d\n", getuid(), getgid());
    } else
    {
    struct passwd *pw = getpwnam(user);  // Use "nobody" or another low-privilege user
    if (!pw)
    {
        fprintf(stderr, "Failed to find '%s' user\n",user);
        return 0;
    }

    // Drop group privileges first
    if (setgid(pw->pw_gid) != 0)
    {
        fprintf(stderr, "Failed to set GID to %d\n", pw->pw_gid);
        return 0;
    }

    // Drop user privileges
    if (setuid(pw->pw_uid) != 0)
    {
        fprintf(stderr, "Failed to set UID to %d\n", pw->pw_uid);
        return 0;
    }

    // Double-check that we're no longer root
    if (geteuid() == 0 || getuid() == 0 || getgid() == 0)
    {
        fprintf(stderr, "Still running as root! Privilege drop failed.\n");
        return 0;
    }

    fprintf(stderr, "Privileges dropped successfully to UID=%d, GID=%d\n", pw->pw_uid, pw->pw_gid);
    }

    return 1;
}

#ifdef __cplusplus
}
#endif



#endif // PERFORMANCE_H_INCLUDED
