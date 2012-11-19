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
#define PID 2
#define VADDR 0x400574



int main(int argc, char *argv[])  
{ 
    int fd, ret, i;  
    unsigned long cr3;
    vmi_instance_t vmi;
    unsigned long offset = 0;

    unsigned long addr = (addr_t) strtoul(argv[2], NULL, 16);

    privcmd_hypercall_t hyper0 = {  
        __HYPERVISOR_change_ept_content, 
        //int domID, unsigned long gfn, unsigned long mfn, int flag, void buff
        { atoi(argv[1]), addr, 0, 6, 0}
    };
    privcmd_hypercall_t hyper1 = {  
        __HYPERVISOR_change_ept_content, 
        //int domID, unsigned long gfn, unsigned long mfn, int flag, void buff
        { atoi(argv[1]), 0, 0, 13, 0}
    };
    fd = open("/proc/xen/privcmd", O_RDWR);  
    if (fd < 0) {  
        perror("open");  
        exit(1);  
    }


    if( atoi(argv[3]) == 1 ) 
        ret = ioctl(fd, IOCTL_PRIVCMD_HYPERCALL, &hyper1);  
    else
        ret = ioctl(fd, IOCTL_PRIVCMD_HYPERCALL, &hyper0);
    return 0;
}
