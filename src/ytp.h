/* ytp.h */

#ifndef __YTP_H__
#define __YTP_H__

/* ---- YTalk protocols ---- */

/*
 * These protocol numbers are a MAJOR HACK designed to get around the fact
 * that old versions of ytalk didn't send any version information during
 * handshaking.  Nor did they bzero() the unused portions of the handshaking
 * structures.  Argh!  These two protocol numbers were very carefully
 * picked... do not add any others and expect them to work. Instead, use the
 * "vmajor" and "vminor" fields of the y_parm structure.
 */
#define YTP_OLD	20		/* YTalk versions before 3.0 */
#define YTP_NEW	27		/* YTalk versions 3.0 and up */

/* ---- Ytalk version 3.* out-of-band data ---- */

/* see comm.c for a description of Ytalk 3.* OOB protocol */

#define V3_OOB		0xfd	/* out-of-band marker -- see comm.c */
#define V3_MAXPACK	0xfc	/* max OOB packet size -- see comm.c */
#define V3_NAMELEN	16	/* max username length */
#define V3_HOSTLEN	64	/* max hostname length */

typedef struct {
	ychar code;		/* V3_EXPORT, V3_IMPORT, or V3_ACCEPT */
	char filler[3];
	ylong host_addr;	/* host address */
	ylong pid;		/* process id */
	char name[V3_NAMELEN];	/* user name */
	char host[V3_HOSTLEN];	/* host name */
} v3_pack;

#define V3_PACKLEN	sizeof(v3_pack)
#define V3_EXPORT	101	/* export a user */
#define V3_IMPORT	102	/* import a user */
#define V3_ACCEPT	103	/* accept a connection from a user */

typedef struct {
	ychar code;		/* V3_LOCKF or V3_UNLOCKF */
	char filler[3];
	ylong flags;		/* flags */
} v3_flags;

#define V3_FLAGSLEN	sizeof(v3_flags)
#define V3_LOCKF	111	/* lock my flags */
#define V3_UNLOCKF	112	/* unlock my flags */

typedef struct {
	ychar code;		/* V3_YOURWIN, V3_MYWIN, or V3_REGION */
	char filler[3];
	unsigned short rows, cols;	/* window size */
} v3_winch;

#define V3_WINCHLEN	sizeof(v3_winch)
#define V3_YOURWIN	121	/* your window size changed over here */
#define V3_MYWIN	122	/* my window size changed over here */
#define V3_REGION	123	/* my window region changed over here */

/* ---- Ytalk version 2.* out-of-band data ---- */

#define V2_NAMELEN	12
#define V2_HOSTLEN	64

typedef struct {
	ychar code;		/* one of the V2_?? codes below */
	char filler;
	char name[V2_NAMELEN];	/* user name */
	char host[V2_HOSTLEN];	/* user host */
} v2_pack;

#define V2_PACKLEN	sizeof(v2_pack)
#define V2_EXPORT	130	/* export a user */
#define V2_IMPORT	131	/* import a user */
#define V2_ACCEPT	132	/* accept a connection from a user */
#define V2_AUTO		133	/* accept auto invitation */

#endif /* __YTP_H__ */
