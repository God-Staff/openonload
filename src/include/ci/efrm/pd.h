/*
** Copyright 2005-2014  Solarflare Communications Inc.
**                      7505 Irvine Center Drive, Irvine, CA 92618, USA
** Copyright 2002-2005  Level 5 Networks Inc.
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of version 2 of the GNU General Public License as
** published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/

/****************************************************************************
 * Driver for Solarflare network controllers -
 *          resource management for Xen backend, OpenOnload, etc
 *           (including support for SFE4001 10GBT NIC)
 *
 * This file provides public API for protection domain resource.
 *
 * Copyright 2012-2012: Solarflare Communications Inc,
 *                      9501 Jeronimo Road, Suite 250,
 *                      Irvine, CA 92618, USA
 *
 * Developed and maintained by Solarflare Communications:
 *                      <linux-xen-drivers@solarflare.com>
 *                      <onload-dev@solarflare.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************
 */

#ifndef __CI_EFRM_PD_H__
#define __CI_EFRM_PD_H__


struct efrm_pd;
struct efrm_vf;
struct efrm_resource;
struct efrm_client;
struct efrm_bt_collection;
struct page;
struct efrm_pd_owner_ids;


/* Packed stream fw requires that all buffers have 1MB alignment.
 *
 * However: On PPC we currently have to use a modified firmware that uses a
 * 64KB buffer size because we can't get large aligned DMA mappings on PPC,
 * which are needed for packed stream.  We just have to hope that the
 * correct firmware is being used, because right now we have no way to
 * check or configure the packed stream buffer size.
 */
#ifdef __PPC__
# define EFRM_PD_RX_PACKED_STREAM_MEMORY_ALIGNMENT  (1u << 16)
#else
# define EFRM_PD_RX_PACKED_STREAM_MEMORY_ALIGNMENT  (1u << 20)
#endif


/* Allocate a protection domain.
 *
 * If [vf_opt] is NULL, then [client_opt] must not be NULL.  If [vf_opt] is
 * supplied then [client_opt] is ignored.
 *
 * [phys_addr_mode] determines whether the protection domain will use
 * physical addresses, or virtual addresses translated via the buffer
 * table.
 */
extern int
efrm_pd_alloc(struct efrm_pd **pd_out, struct efrm_client *client_opt,
	      struct efrm_vf *vf_opt, int phys_addr_mode);

extern void
efrm_pd_release(struct efrm_pd *);

extern struct efrm_resource *
efrm_pd_to_resource(struct efrm_pd *);

extern struct efrm_pd *
efrm_pd_from_resource(struct efrm_resource *);

/* Allocate a block of owner ids.  This must be called to create an owner id
 * block for a function before any protection domains are created for that
 * function.
 *
 * The block is large enough to contain n owner ids.  The owner ids themselves
 * will be based on base.  This allows the shared owner id space on siena to
 * be split up, by basing the owner id block for a function on the base VI ID
 * for that function.
 *
 * To release the block call efrm_pd_owner_ids_dtor.
 *
 * Returns NULL if a block could not be allocated.
 */
extern struct efrm_pd_owner_ids *
efrm_pd_owner_ids_ctor(int base, int n);

/* Release a block of owner ids that have been previously allocated through
 * efrm_pd_owner_ids_ctor.
 */
extern void
efrm_pd_owner_ids_dtor(struct efrm_pd_owner_ids* owner_ids);

/* Return the owner-id associated with this PD.  If the protection domain
 * uses physical addressing, then this function returns 0.
 */
extern int
efrm_pd_owner_id(struct efrm_pd *);

/* Set minimum buffer alignment.  All buffers mapped into this pd must
 * meet this requirement.
 */
extern void
efrm_pd_set_min_align(struct efrm_pd *pd, int alignment);


/* Get minimum buffer alignment.
 */
extern int
efrm_pd_get_min_align(struct efrm_pd *pd);

/* Returns a borrowed reference, or NULL.  Reference remains valid as long
 * as the reference to the pd is held.
 */
extern struct efrm_vf *
efrm_pd_get_vf(struct efrm_pd *);

/* Return the PCI device associated with the protection domain. */
struct pci_dev *efrm_pd_get_pci_dev(struct efrm_pd *pd);

/* Return true if a mapping to one protection domain may be re-used by
 * another.  It happens when:
 * - DMA map is the same (for example, same IOMMU domain);
 * - buffer table is not used (physicall address mode).
 */
int efrm_pd_share_dma_mapping(struct efrm_pd *pd, struct efrm_pd *pd1);


/*************************************************************************
 * Common conventions for the efrm_pd_dma_* functions
 *
 * pages and dma_addrs "arrays" has n_pages length.
 * Each (compound, with order "gfp_order") page is mapped to corresponding
 * PCI device, and dma address in stored in dma_addrs "array".
 *
 * user_addrs "array" keeps the NIC addresses (buffer table addresses for
 * non-physical mode and dma addresses for physical mode).
 * Buffer table "page size" is EFHW_NIC_PAGE_SIZE, which is not equal to
 * PAGE_SIZE on some architectures.
 * user_addrs "array" should be long enaugh to store hardware addresses
 * for all hardware pages of EFHW_NIC_PAGE_SIZE size.
 */

/* Map pages of order gfp_order to hardware.
 * In: pd, n_pages, gfp_order, pages.
 * Out: dma_addrs, user_addrs. */
extern int efrm_pd_dma_map(struct efrm_pd *, int n_pages, int gfp_order,
			   struct page **pages, int pages_stride,
			   void *dma_addrs, int dma_addrs_stride,
			   uint64_t *user_addrs, int user_addrs_stride,
			   void (*user_addr_put)(uint64_t, uint64_t *),
			   struct efrm_bt_collection *);

/* Unmap pages previously mapped by efrm_pd_dma_map(). */
extern void efrm_pd_dma_unmap(struct efrm_pd *, int n_pages, int gfp_order,
			      void *dma_addrs, int dma_addrs_stride,
			      struct efrm_bt_collection *);

/* Re-map pages already mapped by efrm_pd_dma_map() after NIC reset.
 * In: pd, n_pages, gfp_order, pages, dma_addrs.
 *     dma_addrs should be the same as returned by efrm_pd_dma_map().
 * Out: user_addrs.
 *      On EF10, buffer table addresses change across NIC reset.
 *
 * Return codes:
 * 0 - remap OK; new hw addresses are in user_addrs array.
 * -ENOSYS - no remapping is necessary, user_addrs is not changed, old
 *  adresses may be used.
 * -errno - error; the mapping is invalidated; user should kill himself.
 */
extern int efrm_pd_dma_remap_bt(struct efrm_pd *pd, int n_pages, int gfp_order,
				dma_addr_t *pci_addrs, int pci_addrs_stride,
				uint64_t *user_addrs, int user_addrs_stride,
				void (*user_addr_put)(uint64_t, uint64_t *),
                                struct efrm_bt_collection *bt_alloc);
#endif /* __CI_EFRM_PD_H__ */
