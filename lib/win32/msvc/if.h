// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MSVC variants of the interface tree macros of lib/if.h
 * Included at the end of lib/if.h, do not include directly.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_MSVC_IF_H
#define _FRR_WIN_MSVC_IF_H

struct vrf;
extern struct interface *if_rb_name_insert(struct vrf *vrf,
					   struct interface *ifp,
					   const char *caller);
extern struct interface *if_rb_name_remove(struct vrf *vrf,
					   struct interface *ifp,
					   const char *caller);
extern struct interface *if_rb_index_insert(struct vrf *vrf,
					    struct interface *ifp,
					    const char *caller);
extern struct interface *if_rb_index_remove(struct vrf *vrf,
					    struct interface *ifp,
					    const char *caller);

#undef IFNAME_RB_INSERT
#define IFNAME_RB_INSERT(v, ifp) if_rb_name_insert((v), (ifp), __func__)
#undef IFNAME_RB_REMOVE
#define IFNAME_RB_REMOVE(v, ifp) if_rb_name_remove((v), (ifp), __func__)
#undef IFINDEX_RB_INSERT
#define IFINDEX_RB_INSERT(v, ifp) if_rb_index_insert((v), (ifp), __func__)
#undef IFINDEX_RB_REMOVE
#define IFINDEX_RB_REMOVE(v, ifp) if_rb_index_remove((v), (ifp), __func__)

#endif /* _FRR_WIN_MSVC_IF_H */
