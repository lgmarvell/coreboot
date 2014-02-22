/*
 * This file is part of the coreboot project.
 *
 * Copyright (c) 2013 Vladimir Serbinenko
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of
 * the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include <arch/pirq_routing.h>

static const struct irq_routing_table intel_irq_routing_table = {
  PIRQ_SIGNATURE,	/* u32 signature */
  PIRQ_VERSION,		/* u16 version */
  32 + 16 * 16,
  0x00, (0x1f << 3) | 0x0,
  0x0000,
  0x8086, 0x3b07,
  0x00000000,
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, }, /* u8 rfu[11] */
  0x20,/* Checksum (has to be set to some value that
				 * would give 0 after the sum of all bytes
				 * for this structure (including checksum).
                                 */
  {
    /* bus,         dev | fn,    { link, bitmap }, { link, bitmap }, { link, bitmap }, { link, bitmap },    slot, rfu */
    { 0x00, (0x00 << 3) | 0x0, { { 0x01, 0x1cf8 }, { 0x02, 0x1cf8 }, { 0x03, 0x1cf8 }, { 0x04, 0x1cf8 }, }, 0x00, 0x00 },
    { 0x00, (0x01 << 3) | 0x0, { { 0x01, 0x1cf8 }, { 0x00, 0xdef8 }, { 0x00, 0xdef8 }, { 0x00, 0xdef8 }, }, 0x00, 0x00 },
    { 0x00, (0x02 << 3) | 0x0, { { 0x01, 0x1cf8 }, { 0x00, 0xdef8 }, { 0x00, 0xdef8 }, { 0x00, 0xdef8 }, }, 0x00, 0x00 },
    { 0x00, (0x16 << 3) | 0x0, { { 0x01, 0x1cf8 }, { 0x02, 0x1cf8 }, { 0x03, 0x1cf8 }, { 0x04, 0x1cf8 }, }, 0x00, 0x00 },
    { 0x00, (0x19 << 3) | 0x0, { { 0x05, 0x1cf8 }, { 0x00, 0xdef8 }, { 0x00, 0xdef8 }, { 0x00, 0xdef8 }, }, 0x00, 0x00 },
    { 0x00, (0x1a << 3) | 0x0, { { 0x05, 0x1cf8 }, { 0x06, 0x1cf8 }, { 0x07, 0x1cf8 }, { 0x08, 0x1cf8 }, }, 0x00, 0x00 },
    { 0x00, (0x1b << 3) | 0x0, { { 0x00, 0xdef8 }, { 0x02, 0x1cf8 }, { 0x00, 0xdef8 }, { 0x00, 0xdef8 }, }, 0x00, 0x00 },
    { 0x00, (0x1c << 3) | 0x0, { { 0x05, 0x1cf8 }, { 0x06, 0x1cf8 }, { 0x07, 0x1cf8 }, { 0x08, 0x1cf8 }, }, 0x00, 0x00 },
    { 0x00, (0x1c << 3) | 0x3, { { 0x05, 0x1cf8 }, { 0x06, 0x1cf8 }, { 0x07, 0x1cf8 }, { 0x08, 0x1cf8 }, }, 0x00, 0x00 },
    { 0x00, (0x1c << 3) | 0x4, { { 0x05, 0x1cf8 }, { 0x06, 0x1cf8 }, { 0x07, 0x1cf8 }, { 0x08, 0x1cf8 }, }, 0x00, 0x00 },
    { 0x00, (0x1d << 3) | 0x0, { { 0x01, 0x1cf8 }, { 0x02, 0x1cf8 }, { 0x03, 0x1cf8 }, { 0x04, 0x1cf8 }, }, 0x00, 0x00 },
    { 0x00, (0x1e << 3) | 0x0, { { 0x01, 0x1cf8 }, { 0x02, 0x1cf8 }, { 0x03, 0x1cf8 }, { 0x04, 0x1cf8 }, }, 0x00, 0x00 },
    { 0x00, (0x1f << 3) | 0x0, { { 0x08, 0x1cf8 }, { 0x01, 0x1cf8 }, { 0x02, 0x1cf8 }, { 0x04, 0x1cf8 }, }, 0x00, 0x00 },
    { 0x00, (0x1f << 3) | 0x2, { { 0x08, 0x1cf8 }, { 0x01, 0x1cf8 }, { 0x02, 0x1cf8 }, { 0x04, 0x1cf8 }, }, 0x00, 0x00 },
    { 0x00, (0x1f << 3) | 0x3, { { 0x08, 0x1cf8 }, { 0x01, 0x1cf8 }, { 0x02, 0x1cf8 }, { 0x04, 0x1cf8 }, }, 0x00, 0x00 },
    { 0x00, (0x1f << 3) | 0x6, { { 0x08, 0x1cf8 }, { 0x01, 0x1cf8 }, { 0x02, 0x1cf8 }, { 0x04, 0x1cf8 }, }, 0x00, 0x00 },
  }
};

unsigned long write_pirq_routing_table(unsigned long addr)
{
	return copy_pirq_routing_table(addr, &intel_irq_routing_table);
}