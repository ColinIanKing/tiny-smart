/*
 * Copyright (C) 2021 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <scsi/sg.h>
#include <scsi/scsi_ioctl.h>

#define SENSE_BUF_SZ		(0x20)
#define BUF_SZ			(0x200)

/*
 *  See https://www.t10.org/ftp/t10/document.04/04-262r8.pdf
 */
#define CBD_OPERATION_CODE	(0xa1) /* Operation code */
#define CBD_PROTOCOL_DMA	(0x06) /* Protocl DMA */
#define CBD_T_LENGTH		(0x02) /* Tx len in SECTOR_COUNT field */
#define CBD_BYT_BLOK		(0x01) /* Tx len in byte blocks */
#define CBD_T_DIR		(0x01) /* Tx direction, device -> client */
#define CBD_CK_COND		(0x00) /* Check condition, disabled */
#define CBD_OFF_LINE		(0x00) /* offline time, 0 seconds */
#define CBD_FEATURES		(0xd0) /* feature: read smart data */
#define CBD_SECTOR_COUNT	(0x01) /* 1 sector to read */
#define CBD_LBA_LOW		(0x00) /* LBA: 0:7 N/A */
#define CBD_LBA_MID		(0x4f) /* LBA: 23:8 magic: 0xc24f */
#define CBD_LBA_HIGH		(0xc2)
#define CBD_DEVICE		(0x00) /* all zero */
#define CBD_COMMAND		(0xb0) /* command: read smart log */
#define CBD_RESVERVED		(0x00) /* N/A */
#define CBD_CONTROL		(0x00)

#define ATTR_FLAG_WARRANTY	(0x01)
#define ATTR_FLAG_OFFLINE	(0x02)
#define ATTR_FLAG_PERFORMANCE	(0x04)
#define ATTR_FLAG_ERROR_RATE	(0x08)
#define ATTR_FLAG_EVENT_COUNT	(0x10)
#define ATTR_FLAG_SELF_PRESERV	(0x20)

/* SMART log raw data value */
typedef struct __attribute__ ((packed)) {
	uint8_t		attr_id;
	uint16_t	attr_flags;
	uint8_t		current_value;
	uint8_t		worst_value;
	uint32_t	data;
	uint16_t	attr_data;
	uint8_t		threshold;
} raw_value_t;

/*
 *  https://en.wikipedia.org/wiki/S.M.A.R.T.#Known_ATA_S.M.A.R.T._attributes
 */
static const char *id_str[256] = {
	[0x01] = "Read Error Rate",
	[0x02] = "Throughput Performance",
	[0x03] = "Spin-Up Time",
	[0x04] = "Start/Stop Count",
	[0x05] = "Reallocated Sectors Count",
	[0x06] = "Read Channel Margin",
	[0x07] = "Seek Error Rate",
	[0x08] = "Seek Time Performance",
	[0x09] = "Power-On Hours",
	[0x0a] = "Spin Retry Count",
	[0x0b] = "Recalibration Retries",
	[0x0c] = "Power Cycle Count",
	[0x0d] = "Soft Read Error Rate",
	[0x16] = "Current Helium Level",
	[0xaa] = "Available Reserved Space",
	[0xab] = "SSD Program Fail Count",
	[0xac] = "SSD Erase Fail Count",
	[0xad] = "SSD Wear Leveling Count",
	[0xae] = "Unexpected Power Loss Count",
	[0xaf] = "Power Loss Protection Failure",
	[0xb0] = "Erase Fail Count",
	[0xb1] = "Wear Range Delta",
	[0xb2] = "Used Reserved Block Count",
	[0xb3] = "Used Reserved Block Count Total",
	[0xb4] = "Unused Reserved Block Count Total",
	[0xb5] = "Program Fail Count Total",
	[0xb6] = "Erase Fail Count",
	[0xb7] = "SATA Downshift Error Count",
	[0xb8] = "End-to-End error",
	[0xb9] = "Head Stability",
	[0xba] = "Induced Op-Vibration Detection",
	[0xbb] = "Reported Uncorrectable Errors",
	[0xbc] = "Command Timeout",
	[0xbd] = "High Fly Writes",
	[0xbe] = "Temperature Difference",
	[0xbf] = "G-sense Error Rate",
	[0xc0] = "Power-off Retract Count",
	[0xc1] = "Load Cycle Count",
	[0xc2] = "Temperature",
	[0xc3] = "Hardware ECC Recovered",
	[0xc4] = "Reallocation Event Count",
	[0xc5] = "Current Pending Sector Count",
	[0xc6] = "(Offline) Uncorrectable Sector Count",
	[0xc7] = "UltraDMA CRC Error Count",
	[0xc8] = "Multi-Zone Error Rate",
	[0xc9] = "Soft Read Error Rate",
	[0xca] = "Data Address Mark errors",
	[0xcb] = "Run Out Cancel",
	[0xcc] = "Soft ECC Correction",
	[0xcd] = "Thermal Asperity Rate",
	[0xce] = "Flying Height",
	[0xcf] = "Spin High Current",
	[0xd0] = "Spin Buzz",
	[0xd1] = "Offline Seek Performance",
	[0xd2] = "Vibration During Write",
	[0xd3] = "Vibration During Write",
	[0xd4] = "Shock During Write",
	[0xdc] = "Disk Shift",
	[0xdd] = "G-Sense Error Rate",
	[0xde] = "Loaded Hours",
	[0xdf] = "Load/Unload Retry Count",
	[0xe0] = "Load Friction",
	[0xe1] = "Load/Unload Cycle Count",
	[0xe2] = "Load 'In'-time",
	[0xe3] = "Torque Amplification Count",
	[0xe4] = "Power-Off Retract Cycle",
	[0xe6] = "GMR Head Amplitude",
	[0xe7] = "Life Left / Temperature",
	[0xe8] = "Endurance Remaining",
	[0xe9] = "Media Wearout Indicator",
	[0xea] = "Average erase count",
	[0xeb] = "Good Block Count",
	[0xf0] = "Head Flying Hours",
	[0xf1] = "Total LBAs Written",
	[0xf2] = "Total LBAs Read",
	[0xf3] = "Total LBAs Written Expanded",
	[0xf4] = "Total LBAs Read Expanded",
	[0xf9] = "NAND Writes (1GiB)",
	[0xfa] = "Read Error Retry Rate",
	[0xfb] = "Minimum Spares Remaining",
	[0xfc] = "Newly Added Bad Flash Block",
	[0xfe] = "Free Fall Protection",
};

static uint8_t cdb[] = {
	CBD_OPERATION_CODE,
	CBD_PROTOCOL_DMA << 1,
	((CBD_T_LENGTH << 0) |
	 (CBD_BYT_BLOK << 1) |
	 (CBD_T_DIR << 3) |
	 (CBD_CK_COND << 5) |
	 (CBD_OFF_LINE << 6)),
	CBD_FEATURES,
	CBD_SECTOR_COUNT,
	CBD_LBA_LOW,
	CBD_LBA_MID,
	CBD_LBA_HIGH,
	CBD_DEVICE,
	CBD_COMMAND,
	CBD_RESVERVED,
	CBD_CONTROL
};

int main(int argc, char **argv)
{
	int fd;
	uint8_t buf[BUF_SZ], sbuf[SENSE_BUF_SZ];
	sg_io_hdr_t sg_io_hdr;
	const raw_value_t *rv = (const raw_value_t *)(buf + 2);
	const raw_value_t *rv_end = (const raw_value_t *)(buf + sizeof(buf));

	if (argc < 2) {
		fprintf(stderr, "dev required\n");
		return EXIT_FAILURE;
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "cannot open %s, errno=%d (%s)\n",
			argv[1], errno, strerror(errno));
		return EXIT_FAILURE;
	}

	memset(&sg_io_hdr, 0, sizeof(sg_io_hdr));
	sg_io_hdr.interface_id = 'S';
	sg_io_hdr.cmd_len = sizeof(cdb);
	sg_io_hdr.mx_sb_len = sizeof(sbuf);
	sg_io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	sg_io_hdr.dxfer_len = sizeof(buf);
	sg_io_hdr.dxferp = buf;
	sg_io_hdr.cmdp = cdb;
	sg_io_hdr.sbp = sbuf;
	sg_io_hdr.timeout = 35000;
	(void)memset(buf, 0, sizeof(buf));

	if (ioctl(fd, SG_IO, &sg_io_hdr) < 0) {
		(void)close(fd);
		fprintf(stderr, "SG_IO ioctl failed, errno=%d (%s)\n",
			errno, strerror(errno));
		return EXIT_FAILURE;
	}
	(void)close(fd);

	printf("%2s %-30.30s %4s %6.6s %3s %3s %11s %3s\n",
		"ID", "Attribute", "Flgs", "Flags", "Cur", "Wor", "Data", "Thr");

	while ((rv < rv_end) && rv->attr_id) {
		printf("%2x %-30.30s %4x %c%c%c%c%c%c  %2x  %2x %11" PRIu32 "  %2x\n",
			rv->attr_id,
			id_str[rv->attr_id] ? id_str[rv->attr_id] : "?",
			rv->attr_flags,
			(rv->attr_flags & ATTR_FLAG_WARRANTY) ? 'w' : ' ',
			(rv->attr_flags & ATTR_FLAG_OFFLINE) ? 'o' : ' ',
			(rv->attr_flags & ATTR_FLAG_PERFORMANCE) ? 'p' : ' ',
			(rv->attr_flags & ATTR_FLAG_ERROR_RATE) ? 'e' : ' ',
			(rv->attr_flags & ATTR_FLAG_EVENT_COUNT) ? 'c' : ' ',
			(rv->attr_flags & ATTR_FLAG_SELF_PRESERV) ? 's' : ' ',
			rv->current_value,
			rv->worst_value,
			rv->data,
			rv->threshold);
		rv++;
	}

	printf("\nKey:\n");
	printf("  Cur: Current, Wor: Worst, Thr: Threshold\n");
	printf("Flags:\n  w = warranty, o = offline, p = performance, e = error rate\n");
	printf("  c = event code, s = self preservation\n");

	return EXIT_SUCCESS;
}
