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


#define DOMAIN "Web"



int main(int argc, char *argv[])  
{ 
    int fd, ret, i;  
    unsigned long cr3;
    vmi_instance_t vmi;

    unsigned char *memory = NULL;
    uint32_t offset;
    addr_t next_process, list_head;
    char *procname = NULL;
    int pid = 0;
    int tasks_offset, pid_offset, name_offset;
    status_t status;
    unsigned long buff[10];

    /*Start libvmi for getting CR3*/
    if (vmi_init(&vmi, VMI_AUTO | VMI_INIT_COMPLETE, DOMAIN) == VMI_FAILURE){
        printf("LibVmi failure\n");
        exit(1);
    }



    /* init the offset values */
    if (VMI_OS_LINUX == vmi_get_ostype(vmi)){
        tasks_offset = vmi_get_offset(vmi, "linux_tasks");
        name_offset = vmi_get_offset(vmi, "linux_name");
        pid_offset = vmi_get_offset(vmi, "linux_pid");

        /* NOTE:
         *  name_offset is no longer hard-coded. Rather, it is now set
         *  via libvmi.conf.
         */
    }
    else if (VMI_OS_WINDOWS == vmi_get_ostype(vmi)){
        tasks_offset = vmi_get_offset(vmi, "win_tasks");
        if (0 == tasks_offset) {
            printf("Failed to find win_tasks\n");
            goto error_exit;
        }
        name_offset = vmi_get_offset(vmi, "win_pname");
        if (0 == tasks_offset) {
            printf("Failed to find win_pname\n");
            goto error_exit;
        }
        pid_offset = vmi_get_offset(vmi, "win_pid");
        if (0 == tasks_offset) {
            printf("Failed to find win_pid\n");
            goto error_exit;
        }
    }



    /* pause the vm for consistent memory access */
    if (vmi_pause_vm(vmi) != VMI_SUCCESS) {
        printf("Failed to pause VM\n");
        goto error_exit;
    } // if

    fd = open("/proc/xen/privcmd", O_RDWR);  
    if (fd < 0) {  
        perror("open");  
        exit(1);          
    }

    privcmd_hypercall_t hyper = {  
        __HYPERVISOR_change_ept_content, 
        //int domID, unsigned long gfn, unsigned long mfn, int flag, void buff
        { atoi(argv[1]), 0, 0, 5, 0}
    };
    ret = ioctl(fd, IOCTL_PRIVCMD_HYPERCALL, &hyper);  


       /* get the head of the list */
    if (VMI_OS_LINUX == vmi_get_ostype(vmi)){
        addr_t init_task_va = vmi_translate_ksym2v(vmi, "init_task");
        vmi_read_addr_va(vmi, init_task_va + tasks_offset, 0, &next_process);
    }
    else if (VMI_OS_WINDOWS == vmi_get_ostype(vmi)){

        uint32_t pdbase = 0;

        // find PEPROCESS PsInitialSystemProcess
        vmi_read_addr_ksym(vmi, "PsInitialSystemProcess", &list_head);

        vmi_read_addr_va(vmi, list_head + tasks_offset, 0, &next_process);
        vmi_read_32_va(vmi, list_head + pid_offset, 0, &pid);

        vmi_read_32_va(vmi, list_head + pid_offset, 0, &pid);
        procname = vmi_read_str_va(vmi, list_head + name_offset, 0);
        if (!procname) {
            printf ("Failed to find first procname\n");
            goto error_exit;
        }
//        printf("[%5d] %s\n", pid, procname);
        if (procname){
            free(procname);
            procname = NULL;
        }
    }


    list_head = next_process;
    while (1){

        /* follow the next pointer */
        addr_t tmp_next = 0;
        vmi_read_addr_va(vmi, next_process, 0, &tmp_next);
        /* if we are back at the list head, we are done */
        if (list_head == tmp_next){
            break;
        }
        vmi_read_32_va(vmi, next_process + pid_offset - tasks_offset, 0, &pid);
        if( pid>=0 ){
            cr3 = vmi_pid_to_dtb(vmi, pid);
            if(cr3!=0){
//                printf("cr3 %lx\n", cr3);
                privcmd_hypercall_t hyper0 = {  
                    __HYPERVISOR_change_ept_content, 
                    //int domID, unsigned long gfn, unsigned long mfn, int flag, void buff
                    { atoi(argv[1]), cr3, 0, 6, buff}
                };
                ret = ioctl(fd, IOCTL_PRIVCMD_HYPERCALL, &hyper0);  
            }
        }

        next_process = tmp_next;
    }

error_exit:
    /* resume the vm */
    vmi_resume_vm(vmi);

    /* cleanup any memory associated with the LibVMI instance */
    vmi_destroy(vmi);


    return 0;
}
