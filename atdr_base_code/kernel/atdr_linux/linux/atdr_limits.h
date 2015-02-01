/*
  ebdr_limits.h
*/

/*
 * Our current limitations.
 * Some of them are hard limits,
 * some of them are arbitrary range limits, that make it easier to provide
 * feedback about nonsense settings for certain configurable values.
 */

#ifndef EBDR_LIMITS_H
#define EBDR_LIMITS_H 1

#define DEBUG_RANGE_CHECK 0

#define EBDR_MINOR_COUNT_MIN 1
#define EBDR_MINOR_COUNT_MAX 255
#define EBDR_MINOR_COUNT_DEF 32
#define EBDR_MINOR_COUNT_SCALE '1'

#define EBDR_VOLUME_MAX 65535

#define EBDR_DIALOG_REFRESH_MIN 0
#define EBDR_DIALOG_REFRESH_MAX 600
#define EBDR_DIALOG_REFRESH_SCALE '1'

/* valid port number */
#define EBDR_PORT_MIN 1
#define EBDR_PORT_MAX 0xffff
#define EBDR_PORT_SCALE '1'

/* startup { */
  /* if you want more than 3.4 days, disable */
#define EBDR_WFC_TIMEOUT_MIN 0
#define EBDR_WFC_TIMEOUT_MAX 300000
#define EBDR_WFC_TIMEOUT_DEF 0
#define EBDR_WFC_TIMEOUT_SCALE '1'

#define EBDR_DEGR_WFC_TIMEOUT_MIN 0
#define EBDR_DEGR_WFC_TIMEOUT_MAX 300000
#define EBDR_DEGR_WFC_TIMEOUT_DEF 0
#define EBDR_DEGR_WFC_TIMEOUT_SCALE '1'

#define EBDR_OUTDATED_WFC_TIMEOUT_MIN 0
#define EBDR_OUTDATED_WFC_TIMEOUT_MAX 300000
#define EBDR_OUTDATED_WFC_TIMEOUT_DEF 0
#define EBDR_OUTDATED_WFC_TIMEOUT_SCALE '1'
/* }*/

/* net { */
  /* timeout, unit centi seconds
   * more than one minute timeout is not useful */
#define EBDR_TIMEOUT_MIN 1
#define EBDR_TIMEOUT_MAX 600
#define EBDR_TIMEOUT_DEF 60       /* 6 seconds */
#define EBDR_TIMEOUT_SCALE '1'

 /* If backing disk takes longer than disk_timeout, mark the disk as failed */
#define EBDR_DISK_TIMEOUT_MIN 0    /* 0 = disabled */
#define EBDR_DISK_TIMEOUT_MAX 6000 /* 10 Minutes */
#define EBDR_DISK_TIMEOUT_DEF 0    /* disabled */
#define EBDR_DISK_TIMEOUT_SCALE '1'

  /* active connection retries when C_WF_CONNECTION */
#define EBDR_CONNECT_INT_MIN 1
#define EBDR_CONNECT_INT_MAX 120
#define EBDR_CONNECT_INT_DEF 10   /* seconds */
#define EBDR_CONNECT_INT_SCALE '1'

  /* keep-alive probes when idle */
#define EBDR_PING_INT_MIN 1
#define EBDR_PING_INT_MAX 120
#define EBDR_PING_INT_DEF 10
#define EBDR_PING_INT_SCALE '1'

 /* timeout for the ping packets.*/
#define EBDR_PING_TIMEO_MIN  1
#define EBDR_PING_TIMEO_MAX  300
#define EBDR_PING_TIMEO_DEF  5
#define EBDR_PING_TIMEO_SCALE '1'

  /* max number of write requests between write barriers */
#define EBDR_MAX_EPOCH_SIZE_MIN 1
#define EBDR_MAX_EPOCH_SIZE_MAX 20000
#define EBDR_MAX_EPOCH_SIZE_DEF 2048
#define EBDR_MAX_EPOCH_SIZE_SCALE '1'

  /* I don't think that a tcp send buffer of more than 10M is useful */
#define EBDR_SNDBUF_SIZE_MIN  0
#define EBDR_SNDBUF_SIZE_MAX  (10<<20)
#define EBDR_SNDBUF_SIZE_DEF  0
#define EBDR_SNDBUF_SIZE_SCALE '1'

#define EBDR_RCVBUF_SIZE_MIN  0
#define EBDR_RCVBUF_SIZE_MAX  (10<<20)
#define EBDR_RCVBUF_SIZE_DEF  0
#define EBDR_RCVBUF_SIZE_SCALE '1'

  /* @4k PageSize -> 128kB - 512MB */
#define EBDR_MAX_BUFFERS_MIN  32
#define EBDR_MAX_BUFFERS_MAX  131072
#define EBDR_MAX_BUFFERS_DEF  2048
#define EBDR_MAX_BUFFERS_SCALE '1'

  /* @4k PageSize -> 4kB - 512MB */
#define EBDR_UNPLUG_WATERMARK_MIN  1
#define EBDR_UNPLUG_WATERMARK_MAX  131072
#define EBDR_UNPLUG_WATERMARK_DEF (EBDR_MAX_BUFFERS_DEF/16)
#define EBDR_UNPLUG_WATERMARK_SCALE '1'

  /* 0 is disabled.
   * 200 should be more than enough even for very short timeouts */
#define EBDR_KO_COUNT_MIN  0
#define EBDR_KO_COUNT_MAX  200
#define EBDR_KO_COUNT_DEF  7
#define EBDR_KO_COUNT_SCALE '1'
/* } */

/* syncer { */
  /* FIXME allow rate to be zero? */
#define EBDR_RESYNC_RATE_MIN 1
/* channel bonding 10 GbE, or other hardware */
#define EBDR_RESYNC_RATE_MAX (4 << 20)
#define EBDR_RESYNC_RATE_DEF 250
#define EBDR_RESYNC_RATE_SCALE 'k'  /* kilobytes */

  /* less than 7 would hit performance unnecessarily. */
#define EBDR_AL_EXTENTS_MIN  7
  /* we use u16 as "slot number", (u16)~0 is "FREE".
   * If you use >= 292 kB on-disk ring buffer,
   * this is the maximum you can use: */
#define EBDR_AL_EXTENTS_MAX  0xfffe
#define EBDR_AL_EXTENTS_DEF  1237
#define EBDR_AL_EXTENTS_SCALE '1'

#define EBDR_MINOR_NUMBER_MIN  -1
#define EBDR_MINOR_NUMBER_MAX  ((1 << 20) - 1)
#define EBDR_MINOR_NUMBER_DEF  -1
#define EBDR_MINOR_NUMBER_SCALE '1'

/* } */

/* ebdrsetup XY resize -d Z
 * you are free to reduce the device size to nothing, if you want to.
 * the upper limit with 64bit kernel, enough ram and flexible meta data
 * is 1 PiB, currently. */
/* EBDR_MAX_SECTORS */
#define EBDR_DISK_SIZE_MIN  0
#define EBDR_DISK_SIZE_MAX  (1 * (2LLU << 40))
#define EBDR_DISK_SIZE_DEF  0 /* = disabled = no user size... */
#define EBDR_DISK_SIZE_SCALE 's'  /* sectors */

#define EBDR_ON_IO_ERROR_DEF EP_DETACH
#define EBDR_FENCING_DEF FP_DONT_CARE
#define EBDR_AFTER_SB_0P_DEF ASB_DISCONNECT
#define EBDR_AFTER_SB_1P_DEF ASB_DISCONNECT
#define EBDR_AFTER_SB_2P_DEF ASB_DISCONNECT
#define EBDR_RR_CONFLICT_DEF ASB_DISCONNECT
#define EBDR_ON_NO_DATA_DEF OND_IO_ERROR
#define EBDR_ON_CONGESTION_DEF OC_BLOCK
#define EBDR_READ_BALANCING_DEF RB_PREFER_LOCAL

#define EBDR_MAX_BIO_BVECS_MIN 0
#define EBDR_MAX_BIO_BVECS_MAX 128
#define EBDR_MAX_BIO_BVECS_DEF 0
#define EBDR_MAX_BIO_BVECS_SCALE '1'

#define EBDR_C_PLAN_AHEAD_MIN  0
#define EBDR_C_PLAN_AHEAD_MAX  300
#define EBDR_C_PLAN_AHEAD_DEF  20
#define EBDR_C_PLAN_AHEAD_SCALE '1'

#define EBDR_C_DELAY_TARGET_MIN 1
#define EBDR_C_DELAY_TARGET_MAX 100
#define EBDR_C_DELAY_TARGET_DEF 10
#define EBDR_C_DELAY_TARGET_SCALE '1'

#define EBDR_C_FILL_TARGET_MIN 0
#define EBDR_C_FILL_TARGET_MAX (1<<20) /* 500MByte in sec */
#define EBDR_C_FILL_TARGET_DEF 100 /* Try to place 50KiB in socket send buffer during resync */
#define EBDR_C_FILL_TARGET_SCALE 's'  /* sectors */

#define EBDR_C_MAX_RATE_MIN     250
#define EBDR_C_MAX_RATE_MAX     (4 << 20)
#define EBDR_C_MAX_RATE_DEF     102400
#define EBDR_C_MAX_RATE_SCALE	'k'  /* kilobytes */

#define EBDR_C_MIN_RATE_MIN     0
#define EBDR_C_MIN_RATE_MAX     (4 << 20)
#define EBDR_C_MIN_RATE_DEF     250
#define EBDR_C_MIN_RATE_SCALE	'k'  /* kilobytes */

#define EBDR_CONG_FILL_MIN	0
#define EBDR_CONG_FILL_MAX	(10<<21) /* 10GByte in sectors */
#define EBDR_CONG_FILL_DEF	0
#define EBDR_CONG_FILL_SCALE	's'  /* sectors */

#define EBDR_CONG_EXTENTS_MIN	EBDR_AL_EXTENTS_MIN
#define EBDR_CONG_EXTENTS_MAX	EBDR_AL_EXTENTS_MAX
#define EBDR_CONG_EXTENTS_DEF	EBDR_AL_EXTENTS_DEF
#define EBDR_CONG_EXTENTS_SCALE EBDR_AL_EXTENTS_SCALE

#define EBDR_PROTOCOL_DEF EBDR_PROT_C

#define EBDR_DISK_BARRIER_DEF	0
#define EBDR_DISK_FLUSHES_DEF	1
#define EBDR_DISK_DRAIN_DEF	1
#define EBDR_MD_FLUSHES_DEF	1
#define EBDR_TCP_CORK_DEF	1
#define EBDR_AL_UPDATES_DEF     1

#define EBDR_ALLOW_TWO_PRIMARIES_DEF	0
#define EBDR_ALWAYS_ASBP_DEF	0
#define EBDR_USE_RLE_DEF	1

#endif
