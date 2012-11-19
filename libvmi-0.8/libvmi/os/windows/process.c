/* The LibVMI Library is an introspection library that simplifies access to 
 * memory in a target virtual machine or in a file containing a dump of 
 * a system's physical memory.  LibVMI is based on the XenAccess Library.
 *
 * Copyright 2011 Sandia Corporation. Under the terms of Contract
 * DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government
 * retains certain rights in this software.
 *
 * Author: Bryan D. Payne (bdpayne@acm.org)
 *
 * This file is part of LibVMI.
 *
 * LibVMI is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * LibVMI is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with LibVMI.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libvmi.h"
#include "private.h"

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>

char *windows_get_eprocess_name (vmi_instance_t vmi, addr_t paddr)
{
    int name_length = 16; //TODO verify that this is correct for all versions
    addr_t name_paddr = paddr + vmi->os.windows_instance.pname_offset;
    char *name = (char *) safe_malloc(name_length);

    if (name_length == vmi_read_pa(vmi, name_paddr, name, name_length)){
        return name;
    }
    else{
        free(name);
        return NULL;
    }
}

#define MAGIC1 0x1b0003
#define MAGIC2 0x200003
#define MAGIC3 0x580003
static inline int check_magic_2k (uint32_t a) { return (a == MAGIC1); }
static inline int check_magic_xp (uint32_t a) { return (a == MAGIC1); }
static inline int check_magic_2k3 (uint32_t a) { return (a == MAGIC1); }
static inline int check_magic_vista (uint32_t a) { return (a == MAGIC2); }
static inline int check_magic_2k8 (uint32_t a) { return (a == MAGIC1 || a == MAGIC2 || a == MAGIC3); } // not sure what this is, check all
static inline int check_magic_7 (uint32_t a) { return (a == MAGIC3); }
static inline int check_magic_unknown (uint32_t a) { return (a == MAGIC1 || a == MAGIC2 || a == MAGIC3); }

static check_magic_func get_check_magic_func (vmi_instance_t vmi)
{
    check_magic_func rtn = NULL;

    switch (vmi->os.windows_instance.version) {
        case VMI_OS_WINDOWS_2000:
            rtn = &check_magic_2k;
            break;
        case VMI_OS_WINDOWS_XP:
            rtn = &check_magic_xp;
            break;
        case VMI_OS_WINDOWS_2003:
            rtn = &check_magic_2k3;
            break;
        case VMI_OS_WINDOWS_VISTA:
            rtn = &check_magic_vista;
            break;
        case VMI_OS_WINDOWS_2008:
            rtn = &check_magic_2k8;
            break;
        case VMI_OS_WINDOWS_7:
            rtn = &check_magic_7;
            break;
        case VMI_OS_WINDOWS_UNKNOWN:
            rtn = &check_magic_unknown;
            break;
        default:
            rtn = &check_magic_unknown;
            dbprint("--%s: illegal value in vmi->os.windows_instance.version\n", __FUNCTION__);
            break;
    }

    return rtn;
}

int find_pname_offset (vmi_instance_t vmi, check_magic_func check)
{
    addr_t block_pa = 0;
    addr_t offset = 0;
    uint32_t value = 0;
    size_t read = 0;
    void *bm = 0;

    bm = boyer_moore_init("Idle", 4);


#define BLOCK_SIZE 1024 * 1024 * 1
    unsigned char block_buffer[BLOCK_SIZE];

    if (NULL == check){
        check = get_check_magic_func(vmi);
    }

    for (block_pa = 4096; block_pa < vmi->size; block_pa += BLOCK_SIZE){
        read = vmi_read_pa(vmi, block_pa, block_buffer, BLOCK_SIZE);
        if (BLOCK_SIZE != read){
            continue;
        }

        for (offset = 0; offset < BLOCK_SIZE; offset += 8){
            memcpy(&value, block_buffer + offset, 4);

            if (check(value)) { // look for specific magic #
                dbprint("--%s: found magic value 0x%.8x @ offset 0x%.8x\n", __FUNCTION__, value, block_pa + offset);

                unsigned char haystack[0x500];
                read = vmi_read_pa(vmi, block_pa + offset, haystack, 0x500);
                if (0x500 != read){
                    continue;
                }

                int i = boyer_moore2 (bm, haystack, 0x500);

                if (-1 == i){
                    continue;
                }
                else{
                    vmi->init_task = block_pa + offset + vmi->os.windows_instance.tasks_offset;
                    dbprint("--%s: found Idle process at 0x%.8x + 0x%x\n", __FUNCTION__, block_pa + offset, i);
                    boyer_moore_fini (bm);
                    return i;
                }
            }
        }
    }
    boyer_moore_fini (bm);
    return 0;
}

static addr_t find_process_by_name (vmi_instance_t vmi, check_magic_func check, addr_t start_address, const char *name)
{
    addr_t block_pa = 0;
    addr_t offset = 0;
    uint32_t value = 0;
    size_t read = 0;

#define BLOCK_SIZE 1024 * 1024 * 1
    unsigned char block_buffer[BLOCK_SIZE];

    if (NULL == check){
        check = get_check_magic_func(vmi);
    }

    for (block_pa = start_address; block_pa < vmi->size; block_pa += BLOCK_SIZE){
        read = vmi_read_pa(vmi, block_pa, block_buffer, BLOCK_SIZE);
        if (BLOCK_SIZE != read){
            continue;
        }

        for (offset = 0; offset < BLOCK_SIZE; offset += 8){
            memcpy(&value, block_buffer + offset, 4);

            if (check(value)) { // look for specific magic #

                char *procname = windows_get_eprocess_name(vmi, block_pa + offset);
                if (procname){
                    if (strncmp(procname, name, 50) == 0){
                        free(procname);
                        return block_pa + offset;
                    }
                    free(procname);
                }
            }
        }
    }
    return 0;
}

addr_t windows_find_eprocess (vmi_instance_t vmi, char *name)
{
    addr_t start_address = 0;
    check_magic_func check = get_check_magic_func(vmi);

    if (vmi->os.windows_instance.pname_offset == 0){
        vmi->os.windows_instance.pname_offset = find_pname_offset(vmi, check);
        if (vmi->os.windows_instance.pname_offset == 0){
            dbprint("--failed to find pname_offset\n");
            return 0;
        }
        else{
            dbprint("**set os.windows_instance.pname_offset (0x%x)\n", vmi->os.windows_instance.pname_offset);
        }
    }

    if (vmi->init_task){
        start_address = vmi->init_task - vmi->os.windows_instance.tasks_offset;
    }

    return find_process_by_name(vmi, check, start_address, name);
}
