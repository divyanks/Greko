#ifndef EBDR_GENL_STRUCT_H
#define EBDR_GENL_STRUCT_H

/**
 * struct ebdr_genlmsghdr - EBDR specific header used in NETLINK_GENERIC requests
 * @minor:
 *     For admin requests (user -> kernel): which minor device to operate on.
 *     For (unicast) replies or informational (broadcast) messages
 *     (kernel -> user): which minor device the information is about.
 *     If we do not operate on minors, but on connections or resources,
 *     the minor value shall be (~0), and the attribute EBDR_NLA_CFG_CONTEXT
 *     is used instead.
 * @flags: possible operation modifiers (relevant only for user->kernel):
 *     EBDR_GENL_F_SET_DEFAULTS
 * @volume:
 *     When creating a new minor (adding it to a resource), the resource needs
 *     to know which volume number within the resource this is supposed to be.
 *     The volume number corresponds to the same volume number on the remote side,
 *     whereas the minor number on the remote side may be different
 *     (union with flags).
 * @ret_code: kernel->userland unicast cfg reply return code (union with flags);
 */
struct ebdr_genlmsghdr {
	__u32 minor;
	union {
	__u32 flags;
	__s32 ret_code;
	};
};

/* To be used in ebdr_genlmsghdr.flags */
enum {
	EBDR_GENL_F_SET_DEFAULTS = 1,
};

enum ebdr_state_info_bcast_reason {
	SIB_GET_STATUS_REPLY = 1,
	SIB_STATE_CHANGE = 2,
	SIB_HELPER_PRE = 3,
	SIB_HELPER_POST = 4,
	SIB_SYNC_PROGRESS = 5,
};

/* hack around predefined gcc/cpp "linux=1",
 * we cannot possibly include <1/ebdr_genl.h> */
#undef linux

#include <linux/ebdr.h>
#define GENL_MAGIC_VERSION	API_VERSION
#define GENL_MAGIC_FAMILY	ebdr
#define GENL_MAGIC_FAMILY_HDRSZ	sizeof(struct ebdr_genlmsghdr)
#define GENL_MAGIC_INCLUDE_FILE <linux/ebdr_genl.h>
#include <linux/genl_magic_struct.h>

#endif