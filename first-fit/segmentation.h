#ifndef _SEGMENTATION_H_
#define _SEGMENTATION_H_
/*////////////////////////////////////////////////////////////////
//	text, reset, and exception segments
////////////////////////////////////////////////////////////////*/

#define	TEXT_BASE	0xf0000000
#define	TEXT_SIZE	0x00100000

#define	PPC_BOOT_BASE	0xffffff80
#define	PPC_BOOT_SIZE	0x00000080

/*/////////////////////////////////////////////////////////////////
//	local cluster data segment (initialised)
//      !! when simulating > 32 core adjust stacksize in ldscript !!
////////////////////////////////////////////////////////////////*/

#define	DATA_BASE	0xf1000000
#define	DATA_SIZE	0x00100000

#define LOCAL_CACHED_BASE	0xf2000000
#define LOCAL_CACHED_SIZE	0x00100000

#define LOCAL_UNCACHED_BASE	0xf3000000
#define LOCAL_UNCACHED_SIZE	0x00100000

/*/////////////////////////////////////////////////////////
//	System devices
//////////////////////////////////////////////////////////*/

#define	TTY_BASE	0xFDC00000
#define	TTY_SIZE	0x00010000

#define	SIMHELPER_BASE	0xB0200000
#define	SIMHELPER_SIZE	0x00000100

#define FD_BASE     0xFFE00000
#define FD_SIZE     0x00000100

#define SIC_BASE           0x1b000000
#define SIC_SIZE           0x00010000

#define IO_BASE            0x18000000
#define IO_SIZE            0x00010000

#define MIC_BASE           0x1a000000
#define MIC_SIZE           0x00001000

#define SYNC_BASE          0x19000000
#define SYNC_SIZE          0x00000010

#define CAN1_BASE          0x1e000000
#define CAN2_BASE          0x1c000000
#define CAN4_BASE          0x1d000000
#define CAN_SIZE           0x00010000

#define EEPROM_BASE 	   0x26a00000
#define EEPROM_SIZE 	   0x00040000


/////////////////////////////////////////////////////////////////
//	physical segments (derived from size of logical sections)
/////////////////////////////////////////////////////////////////
//
// Each cluster memory starts at an adress that is a multiple of 0x08000000 = 128 MB
// The cluster memory is distributed as follows:
//   * first, the private memories (text and data) for each core
//     (mapped to TEXT_BASE and DATA_BASE)
//   * then the shared cluster memory (mapped to LOCAL_BASE)
//   * at the end the message passing buffer (MPB), accessable from anywhere




#define MEMGAP_CLUSTER	        0x08000000
#define MEMSIZE_CLUSTER(cl)     0x00400000
#define MEMBASE_CLUSTER(cl)     (0x10000000+(cl)*MEMGAP_CLUSTER)

#define MEMSIZE_CORE	        (TEXT_SIZE+DATA_SIZE)
#define MEMBASE_CORE(cl, co)    (MEMBASE_CLUSTER(cl)+(co)*MEMSIZE_CORE)
#define MEMSIZE_CACHED(cl)      LOCAL_CACHED_SIZE
#define MEMBASE_CACHED(cl)      (MEMBASE_CLUSTER(cl)+0x02000000)
#define MEMSIZE_UNCACHED(cl)    LOCAL_UNCACHED_SIZE
#define MEMBASE_UNCACHED(cl)    (MEMBASE_CLUSTER(cl)+0x03000000)
#define MEMSIZE_MPB(cl)		0x00100000
#define MEMBASE_MPB(cl)		(MEMBASE_CLUSTER(cl)+0x04000000)


// IRQ LOGIC
#define TIC_BASE 0x16000000
#define TIC_SIZE 0x00010000
#define TICBASE_CLUSTER(cl)     (MEMBASE_CLUSTER(cl)+0x05000000)


///////////////////////////////////////////////////////////
//	Convertion
///////////////////////////////////////////////////////////


static inline void fill_translation_table(unsigned *tab,
    unsigned cluster, unsigned core, unsigned cores_per_cluster)
{
    int i;
    tab[0] = MEMBASE_CORE(cluster, core);
    tab[1] = MEMBASE_CORE(cluster, core) + TEXT_SIZE;
    tab[2] = MEMBASE_CACHED(cluster);
    tab[3] = MEMBASE_UNCACHED(cluster);
    for (i=4; i<16; i++)
        tab[i] = (i<<24) | 0xf0000000;
}


static inline unsigned address_translation(unsigned logical,
    unsigned cluster, unsigned core)
{
    if (logical >= 0xf0000000) {
	switch (logical>>24) {
	    case 0xf0: return MEMBASE_CORE(cluster, core) + (logical & 0x00ffffff);
	    case 0xf1: return MEMBASE_CORE(cluster, core) + TEXT_SIZE + (logical & 0x00ffffff);
	    case 0xf2: return MEMBASE_CACHED(cluster) + (logical & 0x00ffffff);
	    case 0xf3: return MEMBASE_UNCACHED(cluster) + (logical & 0x00ffffff);
	}
    }
    return logical;
}

#endif
