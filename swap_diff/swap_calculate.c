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


#define RECENT_CR3_SIZE 100+1
#define TOP_CR3_SIZE 0
#define RECENT_WALK_SIZE 10
#define THRESHOLD 800
#define SAMPLE_PERIOD 1
#define TIMES 1

int main(int argc, char *argv[])  
{ 
    int fd, ret, new_cr3_size, os_type, flag, domID;  
    unsigned long cr3;
    unsigned int times = 0;
    vmi_instance_t vmi;
    unsigned long tot_pages, tot_diff;
    unsigned long *new_cr3;    
    unsigned long *all_cr3, top_cr3[TOP_CR3_SIZE];
    unsigned long i, cr3_index, answer_cr3[TOP_CR3_SIZE+RECENT_WALK_SIZE];
    unsigned long last_diff_swap_num[TOP_CR3_SIZE+RECENT_WALK_SIZE];
    unsigned long cr3_buff[TOP_CR3_SIZE+RECENT_WALK_SIZE];
    long j;


    printf("Enter VM ID: "); 
    scanf("%d", &domID);
    printf("Enter VM Memory Size(M): ");
    scanf("%lu", &tot_pages);
    tot_pages *= 256;
    tot_pages += 1024; //for save space
    printf("Enter os_type (0->Linux, 1->Windows): ");
    scanf("%d", &os_type);



    all_cr3 = (unsigned long *)malloc(sizeof(unsigned long)*tot_pages);
    if(all_cr3 == NULL){
        goto ERROR_EXIT;
    }
    for(i=0; i<tot_pages; i++){
        all_cr3[i] = 0;
    }    
    for(i=0; i<TOP_CR3_SIZE; i++)
        top_cr3[i] = 0;
    fd = open("/proc/xen/privcmd", O_RDWR);  
    if (fd < 0) {  
        perror("open");  
        goto ERROR_EXIT;
    }
    privcmd_hypercall_t hyper0 = { //init evironment 
        __HYPERVISOR_change_ept_content, 
        { domID, os_type, 0, 13, 0}
    };
    ret = ioctl(fd, IOCTL_PRIVCMD_HYPERCALL, &hyper0);    //init environment 
    if( ret == -1)
        goto ERROR_EXIT;
    new_cr3 = (unsigned long *)malloc(sizeof(unsigned long)*RECENT_CR3_SIZE);    
    if(new_cr3 == NULL){
        goto ERROR_EXIT;
    }

    for(i=0; i<TOP_CR3_SIZE+RECENT_WALK_SIZE; i++){
        answer_cr3[i] = 0;
    }
    flag = 0;
    while(1){
        for(i=0; i<RECENT_CR3_SIZE; i++){
            new_cr3[i] = 0;            
        }      

        privcmd_hypercall_t hyper1 = { //copy new_cr3 
            __HYPERVISOR_change_ept_content, 
            { domID, os_type, 3, 7, new_cr3}
        };
        ret = ioctl(fd, IOCTL_PRIVCMD_HYPERCALL, &hyper1); //copy cr3 to new_cr3

        new_cr3_size = new_cr3[0];
        for(i=1; i<new_cr3_size+1; i++){ //update top_cr3
            cr3_index = (new_cr3[i]>>12);
            if(cr3_index < tot_pages && cr3_index!=0 ){
                all_cr3[cr3_index]++;
                for(j=(TOP_CR3_SIZE-1); j>=0; j--){ 
                    if( all_cr3[cr3_index] > top_cr3[j] ){
                        if( j!=TOP_CR3_SIZE-1 ){
                            top_cr3[j+1] = top_cr3[j];
                            top_cr3[j] = new_cr3[i]; 
                        }
                        else{
                            top_cr3[j] = new_cr3[i];
                        }
                    }
                }
            }     
        }// end update top_cr3
      
        for(i=0; i<TOP_CR3_SIZE; i++){ 
            answer_cr3[i] = top_cr3[i];
            cr3_buff[i] = top_cr3[i];
        }
        for(i=0; i<RECENT_WALK_SIZE; i++){
            if( i<new_cr3_size ){
                answer_cr3[i+TOP_CR3_SIZE] = new_cr3[i+1];
                cr3_buff[i+TOP_CR3_SIZE] = new_cr3[i+1];
            }
            else{
                answer_cr3[i+TOP_CR3_SIZE] = 0;
                cr3_buff[i+TOP_CR3_SIZE] = 0;
            }
        }

        privcmd_hypercall_t hyper2 = { //walk page tables
            __HYPERVISOR_change_ept_content, 
            { domID, TOP_CR3_SIZE+RECENT_WALK_SIZE, 0, 12, answer_cr3}
        };
        ret = ioctl(fd, IOCTL_PRIVCMD_HYPERCALL, &hyper2); //walk page table


        sleep(SAMPLE_PERIOD);
        
        for(i=0; i<TOP_CR3_SIZE; i++){ 
            last_diff_swap_num[i] = answer_cr3[i];
            answer_cr3[i] = top_cr3[i];            
        }
        for(i=0; i<RECENT_WALK_SIZE; i++){
            last_diff_swap_num[i+TOP_CR3_SIZE] = answer_cr3[i+TOP_CR3_SIZE];
            if( i<new_cr3_size )
                answer_cr3[i+TOP_CR3_SIZE] = new_cr3[i+1];
            else{
                answer_cr3[i+TOP_CR3_SIZE] = 0;
            }
        }

        ret = ioctl(fd, IOCTL_PRIVCMD_HYPERCALL, &hyper2); //walk page table


        tot_diff = 0;
        for(i=0; i<TOP_CR3_SIZE+RECENT_WALK_SIZE; i++){           
            printf("cr3:%lx  answer_cr3[%d]:%lu  last_diff_swap_num[%d]:%lu\n", 
                    cr3_buff[i], i, answer_cr3[i], i, last_diff_swap_num[i]); 
            if(     answer_cr3[i]>last_diff_swap_num[i] 
                    && flag==1
                    && last_diff_swap_num[i] > 0
                    )
            {
                tot_diff += (answer_cr3[i]-last_diff_swap_num[i]);
            }
            else{
//                tot_diff += last_diff_swap_num[i] - answer_cr3[i];
            }
        }

        if(tot_diff > 10)
            printf("Total difference is %lu\n", tot_diff);
        if(tot_diff > THRESHOLD){
            printf("over THRESHOLD\n");
            times++;
        }       
        else{
            times = 0;
        }
        if(times >= TIMES){
            printf("There are bottleneck!!!\n");
            break;
        }

//        printf("If you want to sample input 1: (NEVER type ctrl+c!!!!)\n");        
//        scanf("%d", &flag);
        flag = 1;        
    }



    privcmd_hypercall_t hyper3 = { //empty evironment 
        __HYPERVISOR_change_ept_content, 
        { domID, os_type, 0, 14, 0}
    };
    ret = ioctl(fd, IOCTL_PRIVCMD_HYPERCALL, &hyper3); //empty envionment

    close(fd);
    free(all_cr3);
    free(new_cr3);
    printf("end\n");

ERROR_EXIT:    
    return 0;
}
