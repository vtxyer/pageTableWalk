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
#include <glib.h>
#include <time.h>

#include "glib_compat.h"

struct memory_cache_entry{
    addr_t paddr;
    uint32_t length;
    time_t last_updated;
    time_t last_used;
    void *data;
};
typedef struct memory_cache_entry *memory_cache_entry_t;
static void *(*get_data_callback)(vmi_instance_t, addr_t, uint32_t) = NULL;
static void (*release_data_callback)(void *, size_t) = NULL;

//---------------------------------------------------------
// Internal implementation functions

static void memory_cache_key_free (gpointer data)
{
    if (data) free(data);
}

static void memory_cache_entry_free (gpointer data)
{
    memory_cache_entry_t entry = (memory_cache_entry_t) data;
    if (entry){
        release_data_callback(entry->data, entry->length);
        free(entry);
    }
}

static void *get_memory_data (vmi_instance_t vmi, addr_t paddr, uint32_t length)
{
    return get_data_callback(vmi, paddr, length);
}

static void remove_entry (gpointer key, gpointer cache)
{
    GHashTable *memory_cache = cache;
    g_hash_table_remove(memory_cache, key);
    free(key);
}

static void clean_cache (vmi_instance_t vmi)
{
    GList *list = NULL;
    while (vmi->memory_cache_size > vmi->memory_cache_size_max / 2){
        gpointer key = NULL;
        GList *last = g_list_last(vmi->memory_cache_lru);
        key = last->data;
        list = g_list_prepend(list, key);
        vmi->memory_cache_lru = g_list_remove_link(vmi->memory_cache_lru, last);
        vmi->memory_cache_size--;
    }
    g_list_foreach(list, remove_entry, vmi->memory_cache);
    g_list_free(list);
    dbprint("--MEMORY cache cleanup round complete (cache size = %u)\n", g_hash_table_size(vmi->memory_cache));
}

static void *validate_and_return_data (vmi_instance_t vmi, memory_cache_entry_t entry)
{
    time_t now = time(NULL);
    if (vmi->memory_cache_age && (now - entry->last_updated > vmi->memory_cache_age)){
        dbprint("--MEMORY cache refresh 0x%llx\n", entry->paddr);
		release_data_callback(entry->data, entry->length);
        entry->data = get_memory_data(vmi, entry->paddr, entry->length);
        entry->last_updated = now;

        gint64 *key = safe_malloc(sizeof(gint64));
        *key = entry->paddr;
        vmi->memory_cache_lru = g_list_remove(vmi->memory_cache_lru, key);
        vmi->memory_cache_lru = g_list_prepend(vmi->memory_cache_lru, key);
    }
    entry->last_used = now;
    return entry->data;
}

static memory_cache_entry_t create_new_entry (vmi_instance_t vmi, addr_t paddr, uint32_t length)
{

    // sanity check - are we getting memory outside of the physical memory range?
    // 
    // This does not work with a Xen PV VM during page table lookups, because
    // cr3 > [physical memory size]. It *might* not work when examining a PV
    // snapshot, since we're not sure where the page tables end up. So, we
    // just do it for a HVM guest.
    //
    // TODO: perform other reasonable checks

    if (vmi->hvm && (paddr + length - 1 > vmi->size)) {
        errprint ("--requesting PA [0x%llx] beyond memsize [0x%llx]\n",
                  paddr + length, vmi->size);
        errprint ("\tpaddr: %llx, length %llx, vmi->size %llx\n",
                  paddr, length, vmi->size);
        return 0;
    }

    memory_cache_entry_t entry =
        (memory_cache_entry_t) safe_malloc(sizeof(struct memory_cache_entry));

    entry->paddr        = paddr;
    entry->length       = length;
    entry->last_updated = time(NULL);
    entry->last_used    = entry->last_updated;
    entry->data         = get_memory_data(vmi, paddr, length);

    if (vmi->memory_cache_size >= vmi->memory_cache_size_max){
        clean_cache(vmi);
    }

    return entry;
}

//---------------------------------------------------------
// External API functions
void memory_cache_init (
        vmi_instance_t vmi,
        void *(*get_data)(vmi_instance_t, addr_t, uint32_t),
		void (*release_data)(void *, size_t),
		unsigned long age_limit)
{
    vmi->memory_cache = g_hash_table_new_full(g_int64_hash, g_int64_equal, memory_cache_key_free, memory_cache_entry_free);
    vmi->memory_cache_lru = NULL;
    vmi->memory_cache_age = age_limit;
    vmi->memory_cache_size = 0;
    vmi->memory_cache_size_max = MAX_PAGE_CACHE_SIZE;
    get_data_callback = get_data;
	release_data_callback = release_data;
}

#if ENABLE_PAGE_CACHE == 1
void *memory_cache_insert (vmi_instance_t vmi, addr_t paddr)
{
    memory_cache_entry_t entry = NULL;
    addr_t paddr_aligned = paddr & ~( ((addr_t) vmi->page_size) - 1);
    if (paddr != paddr_aligned){
        errprint("Memory cache request for non-aligned page\n");
        return NULL;
    }

    gint64 *key = safe_malloc(sizeof(gint64));
    *key = paddr;
    if ((entry = g_hash_table_lookup(vmi->memory_cache, key)) != NULL){
        dbprint("--MEMORY cache hit 0x%llx\n", paddr);
        free(key);
        return validate_and_return_data(vmi, entry);
    }
    else{
        dbprint("--MEMORY cache set 0x%llx\n", paddr);
        entry = create_new_entry(vmi, paddr, vmi->page_size);
        if (!entry) {
            errprint ("create_new_entry failed\n");
            return 0;
        }

        g_hash_table_insert(vmi->memory_cache, key, entry);

        gint64 *key2 = safe_malloc(sizeof(gint64));
        *key2 = paddr;
        vmi->memory_cache_lru = g_list_prepend(vmi->memory_cache_lru, key2);
        vmi->memory_cache_size++;

        return entry->data;
    }
}
#else
void *memory_cache_insert (vmi_instance_t vmi, addr_t paddr)
{
    return get_memory_data(vmi, paddr, vmi->page_size);
}
#endif

void memory_cache_destroy (vmi_instance_t vmi)
{
    uint32_t tmp = vmi->memory_cache_size_max;
    vmi->memory_cache_size_max = 0;
    clean_cache(vmi);
    vmi->memory_cache_size_max = tmp;
}
