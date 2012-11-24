#include <stdio.h>  
#include <stdlib.h>  
#include <errno.h>  
#include <sys/ioctl.h>  
#include <sys/types.h>  
#include <fcntl.h>  
#include <string.h>  
#include <xenctrl.h>  
#include <xen/sys/privcmd.h> 
#include <unistd.h>
#include <libvmi/libvmi.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>

#define RECENT_CR3_SIZE 100+1

#define REFRESH_PERIOD 10
#define SHOW_PERIOD (200*1000) //micro seconds
#define SAMPLE_TIMES 1
#define THRESHOLD 200
#define RESAMPLE_PERIOD 3


int fd, ret, ary_size, os_type, flag, domID;  

void end_process(){
    privcmd_hypercall_t hyper2 = { //empty environment 
        __HYPERVISOR_change_ept_content, 
        { domID, os_type, 0, 17, 0}
    };
    ret = ioctl(fd, IOCTL_PRIVCMD_HYPERCALL, &hyper2);
    privcmd_hypercall_t hyper3 = { //empty environment 
        __HYPERVISOR_change_ept_content, 
        { domID, os_type, 0, 14, 0}
    };
    ret = ioctl(fd, IOCTL_PRIVCMD_HYPERCALL, &hyper3);
    printf("over\n");
    exit(0);
}


int main(int argc, char *argv[])  
{ 

    unsigned long total_diff;
    unsigned long tot_pages, tot_swap;
    unsigned long *swap_diff, *last_swap_diff;
    long i;
    long j;

    signal(SIGINT, end_process);
       
    swap_diff = (unsigned long *)malloc(sizeof(unsigned long)*RECENT_CR3_SIZE);
    last_swap_diff = (unsigned long *)malloc(sizeof(unsigned long)*RECENT_CR3_SIZE);

    if(swap_diff ==NULL || last_swap_diff==NULL){
        printf("allocate error\n");
        exit(1);
    }

    printf("Enter VM ID: "); 
    scanf("%d", &domID);
    printf("Enter os_type (0->Linux, 1->Windows): ");
    scanf("%d", &os_type);

    fd = open("/proc/xen/privcmd", O_RDWR);
    if (fd < 0) {
        perror("open");
        exit(1);
    }


    privcmd_hypercall_t hyper0 = { //init environment 
        __HYPERVISOR_change_ept_content, 
        { domID, os_type, 0, 13, 0}
    };
    ret = ioctl(fd, IOCTL_PRIVCMD_HYPERCALL, &hyper0);    //init environment

    flag = 0;

    int show_times, sample_times, bottleneck_flag;

    bottleneck_flag = 0;
    while( 1 ){    

        for(i=0; i<RECENT_CR3_SIZE; i++){
             swap_diff[i] = 0;                
        }
        show_times = (REFRESH_PERIOD*1000*1000)/SHOW_PERIOD;

        privcmd_hypercall_t hyper0 = { //refresh
            __HYPERVISOR_change_ept_content, 
            { domID, os_type, 0, 15, swap_diff}
        };
        ret = ioctl(fd, IOCTL_PRIVCMD_HYPERCALL, &hyper0);

        sample_times = SAMPLE_TIMES;


RESAMPLE:{  


        privcmd_hypercall_t hyper1 = { //re-sample
            __HYPERVISOR_change_ept_content, 
            { domID, os_type, 0, 18, swap_diff}
        };
        ret = ioctl(fd, IOCTL_PRIVCMD_HYPERCALL, &hyper1);

        while(show_times > 0){
            for(i=0; i<RECENT_CR3_SIZE; i++){
//                last_swap_diff[i] = swap_diff[i];
                swap_diff[i] = 0;                
            }
            privcmd_hypercall_t hyper2 = { //copy answer to buff
                __HYPERVISOR_change_ept_content, 
                { domID, os_type, 0, 16, swap_diff}
            };
            ret = ioctl(fd, IOCTL_PRIVCMD_HYPERCALL, &hyper2);

            total_diff = 0;
            ary_size = swap_diff[0];
            for(i=1; i<ary_size+1; i++){
//                total_diff += swap_diff[i] - last_swap_diff[i];
                total_diff += swap_diff[i];
            }



            if( total_diff > THRESHOLD )
            {
                sample_times--;
                if(sample_times == 0){
                    printf("There are bottleneck %lu ", total_diff);
                    for(i=0; i<RECENT_CR3_SIZE; i++){
                        swap_diff[i] = 0;                
                    }
                    privcmd_hypercall_t hyper3 = { //copy swap_num to buff
                        __HYPERVISOR_change_ept_content, 
                        { domID, os_type, 0, 19, swap_diff}
                    };
                    ret = ioctl(fd, IOCTL_PRIVCMD_HYPERCALL, &hyper3);
                    tot_swap = 0;
                    ary_size = swap_diff[0];
                    for(i=1; i<ary_size+1; i++){
                        tot_swap += swap_diff[i];
                    } 
                    printf("you have to add extra %lu[M] memory\n", tot_swap/256);
//                    sleep(RESAMPLE_PERIOD);
//                    goto RESAMPLE;
                }
                else{
//                    printf("into RE-Sample\n");
//                    goto RESAMPLE;
                }
            }

            show_times--;
            usleep(SHOW_PERIOD);
        }
}

    }

END:{
    end_process();

    close(fd);
    free(swap_diff);
    free(last_swap_diff);
    return 0;
    }
}
