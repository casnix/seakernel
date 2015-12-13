#ifndef __MOD_SATA_H
#define __MOD_SATA_H
#include <sea/config.h>
#if CONFIG_MODULE_AHCI
#include <sea/types.h>
#include <sea/fs/inode.h>
#include <sea/mm/dma.h>
#include <sea/dm/blockdev.h>
typedef enum
{
	FIS_TYPE_REG_H2D	= 0x27,	// Register FIS - host to device
	FIS_TYPE_REG_D2H	= 0x34,	// Register FIS - device to host
	FIS_TYPE_DMA_ACT	= 0x39,	// DMA activate FIS - device to host
	FIS_TYPE_DMA_SETUP	= 0x41,	// DMA setup FIS - bidirectional
	FIS_TYPE_DATA		= 0x46,	// Data FIS - bidirectional
	FIS_TYPE_BIST		= 0x58,	// BIST activate FIS - bidirectional
	FIS_TYPE_PIO_SETUP	= 0x5F,	// PIO setup FIS - device to host
	FIS_TYPE_DEV_BITS	= 0xA1,	// Set device bits FIS - device to host
} FIS_TYPE;

struct fis_reg_host_to_device {
	uint8_t	fis_type;
	
	uint8_t pmport:4;
	uint8_t reserved0:3;
	uint8_t c:1;
	
	uint8_t command;
	uint8_t feature_l;
	
	uint8_t lba0;
	uint8_t lba1;
	uint8_t lba2;
	uint8_t device;
	
	uint8_t lba3;
	uint8_t lba4;
	uint8_t lba5;
	uint8_t feature_h;
	
	uint8_t count_l;
	uint8_t count_h;
	uint8_t icc;
	uint8_t control;
	
	uint8_t reserved1[4];
}__attribute__ ((packed));

struct fis_reg_device_to_host {
	uint8_t fis_type;
	
	uint8_t pmport:4;
	uint8_t reserved0:2;
	uint8_t interrupt:1;
	uint8_t reserved1:1;
	
	uint8_t status;
	uint8_t error;
	
	uint8_t lba0;
	uint8_t lba1;
	uint8_t lba2;
	uint8_t device;
	
	uint8_t lba3;
	uint8_t lba4;
	uint8_t lba5;
	uint8_t reserved2;
	
	uint8_t count_l;
	uint8_t count_h;
	uint8_t reserved3[2];
	
	uint8_t reserved4[4];
}__attribute__ ((packed));

struct fis_data {
	uint8_t fis_type;
	uint8_t pmport:4;
	uint8_t reserved0:4;
	uint8_t reserved1[2];
	
	uint32_t data[1];
}__attribute__ ((packed));

struct fis_pio_setup {
	uint8_t fis_type;
	
	uint8_t pmport:4;
	uint8_t reserved0:1;
	uint8_t direction:1;
	uint8_t interrupt:1;
	uint8_t reserved1:1;
	
	uint8_t status;
	uint8_t error;
	
	uint8_t lba0;
	uint8_t lba1;
	uint8_t lba2;
	uint8_t device;
	
	uint8_t lba3;
	uint8_t lba4;
	uint8_t lba5;
	uint8_t reserved2;
	
	uint8_t count_l;
	uint8_t count_h;
	uint8_t reserved3;
	uint8_t e_status;
	
	uint16_t transfer_count;
	uint8_t reserved4[2];
}__attribute__ ((packed));

struct fis_dma_setup {
	uint8_t fis_type;
	
	uint8_t pmport:4;
	uint8_t reserved0:1;
	uint8_t direction:1;
	uint8_t interrupt:1;
	uint8_t auto_activate:1;
	
	uint8_t reserved1[2];
	
	uint64_t dma_buffer_id;
	
	uint32_t reserved2;
	
	uint32_t dma_buffer_offset;
	
	uint32_t transfer_count;
	
	uint32_t reserved3;
}__attribute__ ((packed));

struct fis_dev_bits {
	volatile uint8_t fis_type;
	
	volatile uint8_t pmport:4;
	volatile uint8_t reserved0:2;
	volatile uint8_t interrupt:1;
	volatile uint8_t notification:1;
	
	volatile uint8_t status;
	volatile uint8_t error;
	
	volatile uint32_t protocol;
}__attribute__ ((packed));

struct hba_port {
	volatile uint32_t command_list_base_l;
	volatile uint32_t command_list_base_h;
	volatile uint32_t fis_base_l;
	volatile uint32_t fis_base_h;
	volatile uint32_t interrupt_status;
	volatile uint32_t interrupt_enable;
	volatile uint32_t command;
	volatile uint32_t reserved0;
	volatile uint32_t task_file_data;
	volatile uint32_t signature;
	volatile uint32_t sata_status;
	volatile uint32_t sata_control;
	volatile uint32_t sata_error;
	volatile uint32_t sata_active;
	volatile uint32_t command_issue;
	volatile uint32_t sata_notification;
	volatile uint32_t fis_based_switch_control;
	volatile uint32_t reserved1[11];
	volatile uint32_t vendor[4];
}__attribute__ ((packed));

struct hba_memory {
	volatile uint32_t capability;
	volatile uint32_t global_host_control;
	volatile uint32_t interrupt_status;
	volatile uint32_t port_implemented;
	volatile uint32_t version;
	volatile uint32_t ccc_control;
	volatile uint32_t ccc_ports;
	volatile uint32_t em_location;
	volatile uint32_t em_control;
	volatile uint32_t ext_capabilities;
	volatile uint32_t bohc;
	
	volatile uint8_t reserved[0xA0 - 0x2C];
	
	volatile uint8_t vendor[0x100 - 0xA0];
	
	volatile struct hba_port ports[1];
}__attribute__ ((packed));

struct hba_received_fis {
	volatile struct fis_dma_setup fis_ds;
	volatile uint8_t pad0[4];
	
	volatile struct fis_pio_setup fis_ps;
	volatile uint8_t pad1[12];
	
	volatile struct fis_reg_device_to_host fis_r;
	volatile uint8_t pad2[4];
	
	volatile struct fis_dev_bits fis_sdb;
	volatile uint8_t ufis[64];
	volatile uint8_t reserved[0x100 - 0xA0];
}__attribute__ ((packed));

struct hba_command_header {
	uint8_t fis_length:5;
	uint8_t atapi:1;
	uint8_t write:1;
	uint8_t prefetchable:1;
	
	uint8_t reset:1;
	uint8_t bist:1;
	uint8_t clear_busy_upon_r_ok:1;
	uint8_t reserved0:1;
	uint8_t pmport:4;
	
	uint16_t prdt_len;
	
	volatile uint32_t prdb_count;
	
	uint32_t command_table_base_l;
	uint32_t command_table_base_h;
	
	uint32_t reserved1[4];
}__attribute__ ((packed));

struct hba_prdt_entry {
	uint32_t data_base_l;
	uint32_t data_base_h;
	uint32_t reserved0;
	
	uint32_t byte_count:22;
	uint32_t reserved1:9;
	uint32_t interrupt_on_complete:1;
}__attribute__ ((packed));

struct hba_command_table {
	uint8_t command_fis[64];
	uint8_t acmd[16];
	uint8_t reserved[48];
	struct hba_prdt_entry prdt_entries[1];
}__attribute__ ((packed));

#define AHCI_DEV_SATA 0x00000101
#define HBA_COMMAND_HEADER_NUM 32

struct ata_identify {
	uint16_t ata_device;
	
	uint16_t dont_care[48];
	
	uint16_t cap0;
	uint16_t cap1;
	
	uint16_t obs[2];
	
	uint16_t free_fall;
	
	uint16_t dont_care_2[8];
	
	uint16_t dma_mode0;
	
	uint16_t pio_modes;
	
	uint16_t dont_care_3[4];
	
	uint16_t additional_supported;
	
	uint16_t rsv1[6];
	
	uint16_t serial_ata_cap0;
	
	uint16_t rsv2;
	
	uint16_t serial_ata_features;
	
	uint16_t serial_ata_features_enabled;
	
	uint16_t maj_ver;
	
	uint16_t min_ver;
	
	uint16_t features0;
	
	uint16_t features1;
	
	uint16_t features2;
	
	uint16_t features3;
	
	uint16_t features4;
	
	uint16_t features5;
	
	uint16_t udma_modes;
	
	uint16_t dont_care_4[11];
	
	uint64_t lba48_addressable_sectors;
	
	uint16_t wqewqe[2];
	
	uint16_t ss_1;
	
	uint16_t rrrrr[4];
	
	uint32_t ss_2;
	
	/* ...and more */
};

struct ahci_device {
	uint32_t type;
	int idx, minor;
	struct mutex lock;
	void *fis_virt, *clb_virt;
	struct dma_region dma_clb, dma_fis;
	void *ch[HBA_COMMAND_HEADER_NUM];
	struct dma_region ch_dmas[HBA_COMMAND_HEADER_NUM];
	struct ata_identify identify;
	uint32_t slots;
	int created;
	struct inode *node;
	struct hashelem mapelem;
	struct blockctl bctl;
};

#define HBA_PxCMD_ST  (1 << 0)
#define HBA_PxCMD_CR  (1 << 15)
#define HBA_PxCMD_FR  (1 << 14)
#define HBA_PxCMD_FRE (1 << 4)

#define HBA_GHC_AHCI_ENABLE (1 << 31)
#define HBA_GHC_INTERRUPT_ENABLE (1 << 1)
#define HBA_GHC_RESET (1 << 0)

#define ATA_CMD_IDENTIFY 0xEC

#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08
#define ATA_DEV_ERR 0x01

#define ATA_CMD_READ_DMA_EX 0x25
#define ATA_CMD_WRITE_DMA_EX 0x35

#define PRDT_MAX_COUNT 0x1000

#define PRDT_MAX_ENTRIES 65535

#define ATA_TFD_TIMEOUT  1000000
#define AHCI_CMD_TIMEOUT 1000000

#define ATA_SECTOR_SIZE 512

#define AHCI_DEFAULT_INT 0

struct hba_command_header *ahci_initialize_command_header(struct hba_memory *abar, struct hba_port *port, struct ahci_device *dev, int slot, int write, int atapi, int prd_entries, int fis_len);
struct fis_reg_host_to_device *ahci_initialize_fis_host_to_device(struct hba_memory *abar, struct hba_port *port, struct ahci_device *dev, int slot, int cmdctl, int ata_command);
void ahci_send_command(struct hba_port *port, int slot);
int ahci_write_prdt(struct hba_memory *abar, struct hba_port *port, struct ahci_device *dev, int slot, int offset, int length, addr_t virt_buffer);
int ahci_port_dma_data_transfer(struct hba_memory *abar, struct hba_port *port, struct ahci_device *dev, int slot, int write, addr_t virt_buffer, int sectors, uint64_t lba);
int ahci_device_identify_ahci(struct hba_memory *abar, struct hba_port *port, struct ahci_device *dev);

uint32_t ahci_flush_commands(struct hba_port *port);
void ahci_stop_port_command_engine(volatile struct hba_port *port);
void ahci_start_port_command_engine(volatile struct hba_port *port);
void ahci_reset_device(struct hba_memory *abar, struct hba_port *port, struct ahci_device *dev);
uint32_t ahci_get_previous_byte_count(struct hba_memory *abar, struct hba_port *port, struct ahci_device *dev, int slot);
int ahci_initialize_device(struct hba_memory *abar, struct ahci_device *dev);
uint32_t ahci_check_type(volatile struct hba_port *port);
void ahci_probe_ports(struct hba_memory *abar);
void ahci_init_hba(struct hba_memory *abar);

void ahci_create_device(struct ahci_device *dev);

extern int ahci_int;
extern struct hba_memory *hba_mem;
extern struct ahci_device *ports[32];
extern int ahci_major;

#endif
#endif
