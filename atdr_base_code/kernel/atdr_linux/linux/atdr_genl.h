/*
 * General overview:
 * full generic netlink message:
 * |nlmsghdr|genlmsghdr|<payload>
 *
 * payload:
 * |optional fixed size family header|<sequence of netlink attributes>
 *
 * sequence of netlink attributes:
 * I chose to have all "top level" attributes NLA_NESTED,
 * corresponding to some real struct.
 * So we have a sequence of |tla, len|<nested nla sequence>
 *
 * nested nla sequence:
 * may be empty, or contain a sequence of netlink attributes
 * representing the struct fields.
 *
 * The tag number of any field (regardless of containing struct)
 * will be available as T_ ## field_name,
 * so you cannot have the same field name in two differnt structs.
 *
 * The tag numbers themselves are per struct, though,
 * so should always begin at 1 (not 0, that is the special "NLA_UNSPEC" type,
 * which we won't use here).
 * The tag numbers are used as index in the respective nla_policy array.
 *
 * GENL_struct(tag_name, tag_number, struct name, struct fields) - struct and policy
 *	genl_magic_struct.h
 *		generates the struct declaration,
 *		generates an entry in the tla enum,
 *	genl_magic_func.h
 *		generates an entry in the static tla policy
 *		with .type = NLA_NESTED
 *		generates the static <struct_name>_nl_policy definition,
 *		and static conversion functions
 *
 *	genl_magic_func.h
 *
 * GENL_mc_group(group)
 *	genl_magic_struct.h
 *		does nothing
 *	genl_magic_func.h
 *		defines and registers the mcast group,
 *		and provides a send helper
 *
 * GENL_notification(op_name, op_num, mcast_group, tla list)
 *	These are notifications to userspace.
 *
 *	genl_magic_struct.h
 *		generates an entry in the genl_ops enum,
 *	genl_magic_func.h
 *		does nothing
 *
 *	mcast group: the name of the mcast group this notification should be
 *	expected on
 *	tla list: the list of expected top level attributes,
 *	for documentation and sanity checking.
 *
 * GENL_op(op_name, op_num, flags and handler, tla list) - "genl operations"
 *	These are requests from userspace.
 *
 *	_op and _notification share the same "number space",
 *	op_nr will be assigned to "genlmsghdr->cmd"
 *
 *	genl_magic_struct.h
 *		generates an entry in the genl_ops enum,
 *	genl_magic_func.h
 *		generates an entry in the static genl_ops array,
 *		and static register/unregister functions to
 *		genl_register_family_with_ops().
 *
 *	flags and handler:
 *		GENL_op_init( .doit = x, .dumpit = y, .flags = something)
 *		GENL_doit(x) => .dumpit = NULL, .flags = GENL_ADMIN_PERM
 *	tla list: the list of expected top level attributes,
 *	for documentation and sanity checking.
 */

/*
 * STRUCTS
 */

/* this is sent kernel -> userland on various error conditions, and contains
 * informational textual info, which is supposedly human readable.
 * The computer relevant return code is in the ebdr_genlmsghdr.
 */
GENL_struct(EBDR_NLA_CFG_REPLY, 1, ebdr_cfg_reply,
		/* "arbitrary" size strings, nla_policy.len = 0 */
	__str_field(1, EBDR_GENLA_F_MANDATORY,	info_text, 0)
)

/* Configuration requests typically need a context to operate on.
 * Possible keys are device minor (fits in the ebdr_genlmsghdr),
 * the replication link (aka connection) name,
 * and/or the replication group (aka resource) name,
 * and the volume id within the resource. */
GENL_struct(EBDR_NLA_CFG_CONTEXT, 2, ebdr_cfg_context,
	__u32_field(1, EBDR_GENLA_F_MANDATORY,	ctx_volume)
	__str_field(2, EBDR_GENLA_F_MANDATORY,	ctx_resource_name, 128)
	__bin_field(3, EBDR_GENLA_F_MANDATORY,	ctx_my_addr, 128)
	__bin_field(4, EBDR_GENLA_F_MANDATORY,	ctx_peer_addr, 128)
)

GENL_struct(EBDR_NLA_DISK_CONF, 3, disk_conf,
	__str_field(1, EBDR_F_REQUIRED | EBDR_F_INVARIANT,	backing_dev,	128)
	__str_field(2, EBDR_F_REQUIRED | EBDR_F_INVARIANT,	meta_dev,	128)
	__s32_field(3, EBDR_F_REQUIRED | EBDR_F_INVARIANT,	meta_dev_idx)

	/* use the resize command to try and change the disk_size */
	__u64_field(4, EBDR_GENLA_F_MANDATORY | EBDR_F_INVARIANT,	disk_size)
	/* we could change the max_bio_bvecs,
	 * but it won't propagate through the stack */
	__u32_field(5, EBDR_GENLA_F_MANDATORY | EBDR_F_INVARIANT,	max_bio_bvecs)

	__u32_field_def(6, EBDR_GENLA_F_MANDATORY,	on_io_error, EBDR_ON_IO_ERROR_DEF)
	__u32_field_def(7, EBDR_GENLA_F_MANDATORY,	fencing, EBDR_FENCING_DEF)

	__u32_field_def(8,	EBDR_GENLA_F_MANDATORY,	resync_rate, EBDR_RESYNC_RATE_DEF)
	__s32_field_def(9,	EBDR_GENLA_F_MANDATORY,	resync_after, EBDR_MINOR_NUMBER_DEF)
	__u32_field_def(10,	EBDR_GENLA_F_MANDATORY,	al_extents, EBDR_AL_EXTENTS_DEF)
	__u32_field_def(11,	EBDR_GENLA_F_MANDATORY,	c_plan_ahead, EBDR_C_PLAN_AHEAD_DEF)
	__u32_field_def(12,	EBDR_GENLA_F_MANDATORY,	c_delay_target, EBDR_C_DELAY_TARGET_DEF)
	__u32_field_def(13,	EBDR_GENLA_F_MANDATORY,	c_fill_target, EBDR_C_FILL_TARGET_DEF)
	__u32_field_def(14,	EBDR_GENLA_F_MANDATORY,	c_max_rate, EBDR_C_MAX_RATE_DEF)
	__u32_field_def(15,	EBDR_GENLA_F_MANDATORY,	c_min_rate, EBDR_C_MIN_RATE_DEF)

	__flg_field_def(16, EBDR_GENLA_F_MANDATORY,	disk_barrier, EBDR_DISK_BARRIER_DEF)
	__flg_field_def(17, EBDR_GENLA_F_MANDATORY,	disk_flushes, EBDR_DISK_FLUSHES_DEF)
	__flg_field_def(18, EBDR_GENLA_F_MANDATORY,	disk_drain, EBDR_DISK_DRAIN_DEF)
	__flg_field_def(19, EBDR_GENLA_F_MANDATORY,	md_flushes, EBDR_MD_FLUSHES_DEF)
	__u32_field_def(20,	EBDR_GENLA_F_MANDATORY,	disk_timeout, EBDR_DISK_TIMEOUT_DEF)
	__u32_field_def(21,	0 /* OPTIONAL */,       read_balancing, EBDR_READ_BALANCING_DEF)
	/* 9: __u32_field_def(22,	EBDR_GENLA_F_MANDATORY,	unplug_watermark, EBDR_UNPLUG_WATERMARK_DEF) */
	__flg_field_def(23,     0 /* OPTIONAL */,	al_updates, EBDR_AL_UPDATES_DEF)
)

GENL_struct(EBDR_NLA_RESOURCE_OPTS, 4, res_opts,
	__str_field_def(1,	EBDR_GENLA_F_MANDATORY,	cpu_mask,       32)
	__u32_field_def(2,	EBDR_GENLA_F_MANDATORY,	on_no_data, EBDR_ON_NO_DATA_DEF)
)

GENL_struct(EBDR_NLA_NET_CONF, 5, net_conf,
	__str_field_def(1,	EBDR_GENLA_F_MANDATORY | EBDR_F_SENSITIVE,
						shared_secret,	SHARED_SECRET_MAX)
	__str_field_def(2,	EBDR_GENLA_F_MANDATORY,	cram_hmac_alg,	SHARED_SECRET_MAX)
	__str_field_def(3,	EBDR_GENLA_F_MANDATORY,	integrity_alg,	SHARED_SECRET_MAX)
	__str_field_def(4,	EBDR_GENLA_F_MANDATORY,	verify_alg,     SHARED_SECRET_MAX)
	__str_field_def(5,	EBDR_GENLA_F_MANDATORY,	csums_alg,	SHARED_SECRET_MAX)
	__u32_field_def(6,	EBDR_GENLA_F_MANDATORY,	wire_protocol, EBDR_PROTOCOL_DEF)
	__u32_field_def(7,	EBDR_GENLA_F_MANDATORY,	connect_int, EBDR_CONNECT_INT_DEF)
	__u32_field_def(8,	EBDR_GENLA_F_MANDATORY,	timeout, EBDR_TIMEOUT_DEF)
	__u32_field_def(9,	EBDR_GENLA_F_MANDATORY,	ping_int, EBDR_PING_INT_DEF)
	__u32_field_def(10,	EBDR_GENLA_F_MANDATORY,	ping_timeo, EBDR_PING_TIMEO_DEF)
	__u32_field_def(11,	EBDR_GENLA_F_MANDATORY,	sndbuf_size, EBDR_SNDBUF_SIZE_DEF)
	__u32_field_def(12,	EBDR_GENLA_F_MANDATORY,	rcvbuf_size, EBDR_RCVBUF_SIZE_DEF)
	__u32_field_def(13,	EBDR_GENLA_F_MANDATORY,	ko_count, EBDR_KO_COUNT_DEF)
	__u32_field_def(14,	EBDR_GENLA_F_MANDATORY,	max_buffers, EBDR_MAX_BUFFERS_DEF)
	__u32_field_def(15,	EBDR_GENLA_F_MANDATORY,	max_epoch_size, EBDR_MAX_EPOCH_SIZE_DEF)
	__u32_field_def(16,	EBDR_GENLA_F_MANDATORY,	unplug_watermark, EBDR_UNPLUG_WATERMARK_DEF)
	__u32_field_def(17,	EBDR_GENLA_F_MANDATORY,	after_sb_0p, EBDR_AFTER_SB_0P_DEF)
	__u32_field_def(18,	EBDR_GENLA_F_MANDATORY,	after_sb_1p, EBDR_AFTER_SB_1P_DEF)
	__u32_field_def(19,	EBDR_GENLA_F_MANDATORY,	after_sb_2p, EBDR_AFTER_SB_2P_DEF)
	__u32_field_def(20,	EBDR_GENLA_F_MANDATORY,	rr_conflict, EBDR_RR_CONFLICT_DEF)
	__u32_field_def(21,	EBDR_GENLA_F_MANDATORY,	on_congestion, EBDR_ON_CONGESTION_DEF)
	__u32_field_def(22,	EBDR_GENLA_F_MANDATORY,	cong_fill, EBDR_CONG_FILL_DEF)
	__u32_field_def(23,	EBDR_GENLA_F_MANDATORY,	cong_extents, EBDR_CONG_EXTENTS_DEF)
	__flg_field_def(24, EBDR_GENLA_F_MANDATORY,	two_primaries, EBDR_ALLOW_TWO_PRIMARIES_DEF)
	__flg_field(25, EBDR_GENLA_F_MANDATORY | EBDR_F_INVARIANT,	discard_my_data)
	__flg_field_def(26, EBDR_GENLA_F_MANDATORY,	tcp_cork, EBDR_TCP_CORK_DEF)
	__flg_field_def(27, EBDR_GENLA_F_MANDATORY,	always_asbp, EBDR_ALWAYS_ASBP_DEF)
	__flg_field(28, EBDR_GENLA_F_MANDATORY | EBDR_F_INVARIANT,	tentative)
	__flg_field_def(29,	EBDR_GENLA_F_MANDATORY,	use_rle, EBDR_USE_RLE_DEF)
	/* 9: __u32_field_def(30,	EBDR_GENLA_F_MANDATORY,	fencing_policy, EBDR_FENCING_DEF) */
)

GENL_struct(EBDR_NLA_SET_ROLE_PARMS, 6, set_role_parms,
	__flg_field(1, EBDR_GENLA_F_MANDATORY,	assume_uptodate)
)

GENL_struct(EBDR_NLA_RESIZE_PARMS, 7, resize_parms,
	__u64_field(1, EBDR_GENLA_F_MANDATORY,	resize_size)
	__flg_field(2, EBDR_GENLA_F_MANDATORY,	resize_force)
	__flg_field(3, EBDR_GENLA_F_MANDATORY,	no_resync)
)

GENL_struct(EBDR_NLA_STATE_INFO, 8, state_info,
	/* the reason of the broadcast,
	 * if this is an event triggered broadcast. */
	__u32_field(1, EBDR_GENLA_F_MANDATORY,	sib_reason)
	__u32_field(2, EBDR_F_REQUIRED,	current_state)
	__u64_field(3, EBDR_GENLA_F_MANDATORY,	capacity)
	__u64_field(4, EBDR_GENLA_F_MANDATORY,	ed_uuid)

	/* These are for broadcast from after state change work.
	 * prev_state and new_state are from the moment the state change took
	 * place, new_state is not neccessarily the same as current_state,
	 * there may have been more state changes since.  Which will be
	 * broadcasted soon, in their respective after state change work.  */
	__u32_field(5, EBDR_GENLA_F_MANDATORY,	prev_state)
	__u32_field(6, EBDR_GENLA_F_MANDATORY,	new_state)

	/* if we have a local disk: */
	__bin_field(7, EBDR_GENLA_F_MANDATORY,	uuids, (UI_SIZE*sizeof(__u64)))
	__u32_field(8, EBDR_GENLA_F_MANDATORY,	disk_flags)
	__u64_field(9, EBDR_GENLA_F_MANDATORY,	bits_total)
	__u64_field(10, EBDR_GENLA_F_MANDATORY,	bits_oos)
	/* and in case resync or online verify is active */
	__u64_field(11, EBDR_GENLA_F_MANDATORY,	bits_rs_total)
	__u64_field(12, EBDR_GENLA_F_MANDATORY,	bits_rs_failed)

	/* for pre and post notifications of helper execution */
	__str_field(13, EBDR_GENLA_F_MANDATORY,	helper, 32)
	__u32_field(14, EBDR_GENLA_F_MANDATORY,	helper_exit_code)

	__u64_field(15,                      0, send_cnt)
	__u64_field(16,                      0, recv_cnt)
	__u64_field(17,                      0, read_cnt)
	__u64_field(18,                      0, writ_cnt)
	__u64_field(19,                      0, al_writ_cnt)
	__u64_field(20,                      0, bm_writ_cnt)
	__u32_field(21,                      0, ap_bio_cnt)
	__u32_field(22,                      0, ap_pending_cnt)
	__u32_field(23,                      0, rs_pending_cnt)
)

GENL_struct(EBDR_NLA_START_OV_PARMS, 9, start_ov_parms,
	__u64_field(1, EBDR_GENLA_F_MANDATORY,	ov_start_sector)
	__u64_field(2, EBDR_GENLA_F_MANDATORY,	ov_stop_sector)
)

GENL_struct(EBDR_NLA_NEW_C_UUID_PARMS, 10, new_c_uuid_parms,
	__flg_field(1, EBDR_GENLA_F_MANDATORY, clear_bm)
)

GENL_struct(EBDR_NLA_TIMEOUT_PARMS, 11, timeout_parms,
	__u32_field(1,	EBDR_F_REQUIRED,	timeout_type)
)

GENL_struct(EBDR_NLA_DISCONNECT_PARMS, 12, disconnect_parms,
	__flg_field(1, EBDR_GENLA_F_MANDATORY,	force_disconnect)
)

GENL_struct(EBDR_NLA_DETACH_PARMS, 13, detach_parms,
	__flg_field(1, EBDR_GENLA_F_MANDATORY,	force_detach)
)

/*
 * Notifications and commands (genlmsghdr->cmd)
 */
GENL_mc_group(events)

	/* kernel -> userspace announcement of changes */
GENL_notification(
	EBDR_EVENT, 1, events,
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED)
	GENL_tla_expected(EBDR_NLA_STATE_INFO, EBDR_F_REQUIRED)
	GENL_tla_expected(EBDR_NLA_NET_CONF, EBDR_GENLA_F_MANDATORY)
	GENL_tla_expected(EBDR_NLA_DISK_CONF, EBDR_GENLA_F_MANDATORY)
	GENL_tla_expected(EBDR_NLA_SYNCER_CONF, EBDR_GENLA_F_MANDATORY)
)

	/* query kernel for specific or all info */
GENL_op(
	EBDR_ADM_GET_STATUS, 2,
	GENL_op_init(
		.doit = ebdr_adm_get_status,
		.dumpit = ebdr_adm_get_status_all,
		/* anyone may ask for the status,
		 * it is broadcasted anyways */
	),
	/* To select the object .doit.
	 * Or a subset of objects in .dumpit. */
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_GENLA_F_MANDATORY)
)

	/* add EBDR minor devices as volumes to resources */
GENL_op(EBDR_ADM_NEW_MINOR, 5, GENL_doit(ebdr_adm_new_minor),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED))
GENL_op(EBDR_ADM_DEL_MINOR, 6, GENL_doit(ebdr_adm_delete_minor),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED))

	/* add or delete resources */
GENL_op(EBDR_ADM_NEW_RESOURCE, 7, GENL_doit(ebdr_adm_new_resource),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED))
GENL_op(EBDR_ADM_DEL_RESOURCE, 8, GENL_doit(ebdr_adm_del_resource),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED))

GENL_op(EBDR_ADM_RESOURCE_OPTS, 9,
	GENL_doit(ebdr_adm_resource_opts),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED)
	GENL_tla_expected(EBDR_NLA_RESOURCE_OPTS, EBDR_GENLA_F_MANDATORY)
)

GENL_op(
	EBDR_ADM_CONNECT, 10,
	GENL_doit(ebdr_adm_connect),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED)
	GENL_tla_expected(EBDR_NLA_NET_CONF, EBDR_F_REQUIRED)
)

GENL_op(
	EBDR_ADM_CHG_NET_OPTS, 29,
	GENL_doit(ebdr_adm_net_opts),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED)
	GENL_tla_expected(EBDR_NLA_NET_CONF, EBDR_F_REQUIRED)
)

GENL_op(EBDR_ADM_DISCONNECT, 11, GENL_doit(ebdr_adm_disconnect),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED))

GENL_op(EBDR_ADM_ATTACH, 12,
	GENL_doit(ebdr_adm_attach),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED)
	GENL_tla_expected(EBDR_NLA_DISK_CONF, EBDR_F_REQUIRED)
)

GENL_op(EBDR_ADM_CHG_DISK_OPTS, 28,
	GENL_doit(ebdr_adm_disk_opts),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED)
	GENL_tla_expected(EBDR_NLA_DISK_OPTS, EBDR_F_REQUIRED)
)

GENL_op(
	EBDR_ADM_RESIZE, 13,
	GENL_doit(ebdr_adm_resize),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED)
	GENL_tla_expected(EBDR_NLA_RESIZE_PARMS, EBDR_GENLA_F_MANDATORY)
)

GENL_op(
	EBDR_ADM_PRIMARY, 14,
	GENL_doit(ebdr_adm_set_role),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED)
	GENL_tla_expected(EBDR_NLA_SET_ROLE_PARMS, EBDR_F_REQUIRED)
)

GENL_op(
	EBDR_ADM_SECONDARY, 15,
	GENL_doit(ebdr_adm_set_role),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED)
	GENL_tla_expected(EBDR_NLA_SET_ROLE_PARMS, EBDR_F_REQUIRED)
)

GENL_op(
	EBDR_ADM_NEW_C_UUID, 16,
	GENL_doit(ebdr_adm_new_c_uuid),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED)
	GENL_tla_expected(EBDR_NLA_NEW_C_UUID_PARMS, EBDR_GENLA_F_MANDATORY)
)

GENL_op(
	EBDR_ADM_START_OV, 17,
	GENL_doit(ebdr_adm_start_ov),
	GENL_tla_expected(EBDR_NLA_START_OV_PARMS, EBDR_GENLA_F_MANDATORY)
)

GENL_op(EBDR_ADM_DETACH,	18, GENL_doit(ebdr_adm_detach),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED)
	GENL_tla_expected(EBDR_NLA_DETACH_PARMS, EBDR_GENLA_F_MANDATORY))

GENL_op(EBDR_ADM_INVALIDATE,	19, GENL_doit(ebdr_adm_invalidate),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED))
GENL_op(EBDR_ADM_INVAL_PEER,	20, GENL_doit(ebdr_adm_invalidate_peer),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED))
GENL_op(EBDR_ADM_PAUSE_SYNC,	21, GENL_doit(ebdr_adm_pause_sync),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED))
GENL_op(EBDR_ADM_RESUME_SYNC,	22, GENL_doit(ebdr_adm_resume_sync),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED))
GENL_op(EBDR_ADM_SUSPEND_IO,	23, GENL_doit(ebdr_adm_suspend_io),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED))
GENL_op(EBDR_ADM_RESUME_IO,	24, GENL_doit(ebdr_adm_resume_io),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED))
GENL_op(EBDR_ADM_OUTDATE,	25, GENL_doit(ebdr_adm_outdate),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED))
GENL_op(EBDR_ADM_GET_TIMEOUT_TYPE, 26, GENL_doit(ebdr_adm_get_timeout_type),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED))
GENL_op(EBDR_ADM_DOWN,		27, GENL_doit(ebdr_adm_down),
	GENL_tla_expected(EBDR_NLA_CFG_CONTEXT, EBDR_F_REQUIRED))
