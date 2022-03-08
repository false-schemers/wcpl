/* WASI API: https://github.com/WebAssembly/WASI/ */
/* NB: this is 'real' WASI API (see __wasilibc_real.c) */

#pragma module "wasi_snapshot_preview1"
#pragma once

#define DIRCOOKIE_START 0UL

/* Non-negative file size or length of a region within a file. */
typedef uint64_t filesize_t;

/* Timestamp in nanoseconds. */
typedef uint64_t timestamp_t;

/* Identifiers for clocks. */
typedef uint32_t clockid_t;
/* The clock measuring real time. Time value zero corresponds with
 * 1970-01-01T00:00:00Z. */
#define CLOCKID_REALTIME 0U
/* The store-wide monotonic clock, which is defined as a clock measuring
 * real time, whose value cannot be adjusted and which cannot have negative
 * clock jumps. The epoch of this clock is undefined. The absolute time
 * value of this clock therefore has no meaning. */
#define CLOCKID_MONOTONIC 1U
/* The CPU-time clock associated with the current process. */
#define CLOCKID_PROCESS_CPUTIME_ID 2U
/* The CPU-time clock associated with the current thread. */
#define CLOCKID_THREAD_CPUTIME_ID 3U

/* Error codes returned by functions.
 * Not all of these error codes are returned by the functions provided by this
 * API; some are used in higher-level library layers, and others are provided
 * merely for alignment with POSIX. */
typedef uint16_t errno_t;

#define ERRNO_SUCCESS 0 /* No error occurred. System call completed successfully. */
#define ERRNO_2BIG 1 /* Argument list too long. */
#define ERRNO_ACCES 2 /* Permission denied. */
#define ERRNO_ADDRINUSE 3 /* Address in use. */
#define ERRNO_ADDRNOTAVAIL 4 /* Address not available. */
#define ERRNO_AFNOSUPPORT 5 /* Address family not supported. */
#define ERRNO_AGAIN 6 /* Resource unavailable, or operation would block. */
#define ERRNO_ALREADY 7 /* Connection already in progress. */
#define ERRNO_BADF 8 /* Bad file descriptor. */
#define ERRNO_BADMSG 9 /* Bad message. */
#define ERRNO_BUSY 10 /* Device or resource busy. */
#define ERRNO_CANCELED 11 /* Operation canceled. */
#define ERRNO_CHILD 12 /* No child processes. */
#define ERRNO_CONNABORTED 13 /* Connection aborted. */
#define ERRNO_CONNREFUSED 14 /* Connection refused. */
#define ERRNO_CONNRESET 15 /* Connection reset. */
#define ERRNO_DEADLK 16 /* Resource deadlock would occur. */
#define ERRNO_DESTADDRREQ 17 /* Destination address required. */
#define ERRNO_DOM 18 /* Mathematics argument out of domain of function. */
#define ERRNO_DQUOT 19 /* Reserved. */
#define ERRNO_EXIST 20 /* File exists. */
#define ERRNO_FAULT 21 /* Bad address. */
#define ERRNO_FBIG 22 /* File too large. */
#define ERRNO_HOSTUNREACH 23 /* Host is unreachable. */
#define ERRNO_IDRM 24 /* Identifier removed. */
#define ERRNO_ILSEQ 25 /* Illegal byte sequence. */
#define ERRNO_INPROGRESS 26 /* Operation in progress. */
#define ERRNO_INTR 27 /* Interrupted function. */
#define ERRNO_INVAL 28 /* Invalid argument. */
#define ERRNO_IO 29 /* I/O error. */
#define ERRNO_ISCONN 30 /* Socket is connected. */
#define ERRNO_ISDIR 31 /* Is a directory. */
#define ERRNO_LOOP 32 /* Too many levels of symbolic links. */
#define ERRNO_MFILE 33 /* File descriptor value too large. */
#define ERRNO_MLINK 34 /* Too many links. */
#define ERRNO_MSGSIZE 35 /* Message too large. */
#define ERRNO_MULTIHOP 36 /* Reserved. */
#define ERRNO_NAMETOOLONG 37 /* Filename too long. */
#define ERRNO_NETDOWN 38 /* Network is down. */
#define ERRNO_NETRESET 39 /* Connection aborted by network. */
#define ERRNO_NETUNREACH 40 /* Network unreachable. */
#define ERRNO_NFILE 41 /* Too many files open in system. */
#define ERRNO_NOBUFS 42 /* No buffer space available. */
#define ERRNO_NODEV 43 /* No such device. */
#define ERRNO_NOENT 44 /* No such file or directory. */
#define ERRNO_NOEXEC 45 /* Executable file format error. */
#define ERRNO_NOLCK 46 /* No locks available. */
#define ERRNO_NOLINK 47 /* Reserved. */
#define ERRNO_NOMEM 48 /* Not enough space. */
#define ERRNO_NOMSG 49 /* No message of the desired type. */
#define ERRNO_NOPROTOOPT 50 /* Protocol not available. */
#define ERRNO_NOSPC 51 /* No space left on device. */
#define ERRNO_NOSYS 52 /* Function not supported. */
#define ERRNO_NOTCONN 53 /* The socket is not connected. */
#define ERRNO_NOTDIR 54 /* Not a directory or a symbolic link to a directory. */
#define ERRNO_NOTEMPTY 55 /* Directory not empty. */
#define ERRNO_NOTRECOVERABLE 56 /* State not recoverable. */
#define ERRNO_NOTSOCK 57 /* Not a socket. */
#define ERRNO_NOTSUP 58 /* Not supported, or operation not supported on socket. */
#define ERRNO_NOTTY 59 /* Inappropriate I/O control operation. */
#define ERRNO_NXIO 60 /* No such device or address. */
#define ERRNO_OVERFLOW 61 /* Value too large to be stored in data type. */
#define ERRNO_OWNERDEAD 62 /* Previous owner died. */
#define ERRNO_PERM 63 /* Operation not permitted. */
#define ERRNO_PIPE 64 /* Broken pipe. */
#define ERRNO_PROTO 65 /* Protocol error. */
#define ERRNO_PROTONOSUPPORT 66 /* Protocol not supported. */
#define ERRNO_PROTOTYPE 67 /* Protocol wrong type for socket. */
#define ERRNO_RANGE 68 /* Result too large. */
#define ERRNO_ROFS 69 /* Read-only file system. */
#define ERRNO_SPIPE 70 /* Invalid seek. */
#define ERRNO_SRCH 71 /* No such process. */
#define ERRNO_STALE 72 /* Reserved. */
#define ERRNO_TIMEDOUT 73 /* Connection timed out. */
#define ERRNO_TXTBSY 74 /* Text file busy. */
#define ERRNO_XDEV 75 /* Cross-device link. */
#define ERRNO_NOTCAPABLE 76 /* Extension: Capabilities insufficient. */

/* File descriptor rights, determining which actions may be performed. */
typedef uint64_t rights_t;

/* The right to invoke `fd_datasync`.
 * If `path_open` is set, includes the right to invoke
 * `path_open` with `fdflags::dsync`. */
#define RIGHTS_FD_DATASYNC ((rights_t)(1 << 0))
/* The right to invoke `fd_read` and `sock_recv`.
 * If `rights::fd_seek` is set, includes the right to invoke `fd_pread`. */
#define RIGHTS_FD_READ ((rights_t)(1 << 1))
/* The right to invoke `fd_seek`. This flag implies `rights::fd_tell`. */
#define RIGHTS_FD_SEEK ((rights_t)(1 << 2))
/* The right to invoke `fd_fdstat_set_flags`. */
#define RIGHTS_FD_FDSTAT_SET_FLAGS ((rights_t)(1 << 3))
/* The right to invoke `fd_sync`.
 * If `path_open` is set, includes the right to invoke
 * `path_open` with `fdflags::rsync` and `fdflags::dsync`. */
#define RIGHTS_FD_SYNC ((rights_t)(1 << 4))
/* The right to invoke `fd_seek` in such a way that the file offset
 * remains unaltered (i.e., `whence::cur` with offset zero), or to
 * invoke `fd_tell`. */
#define RIGHTS_FD_TELL ((rights_t)(1 << 5))
/* The right to invoke `fd_write` and `sock_send`.
 * If `rights::fd_seek` is set, includes the right to invoke `fd_pwrite`. */
#define RIGHTS_FD_WRITE ((rights_t)(1 << 6))
/* The right to invoke `fd_advise`. */
#define RIGHTS_FD_ADVISE ((rights_t)(1 << 7))
/* The right to invoke `fd_allocate`. */
#define RIGHTS_FD_ALLOCATE ((rights_t)(1 << 8))
/* The right to invoke `path_create_directory`. */
#define RIGHTS_PATH_CREATE_DIRECTORY ((rights_t)(1 << 9))
/* If `path_open` is set, the right to invoke `path_open` with `oflags::creat`. */
#define RIGHTS_PATH_CREATE_FILE ((rights_t)(1 << 10))
/* The right to invoke `path_link` with the file descriptor as the
 * source directory. */
#define RIGHTS_PATH_LINK_SOURCE ((rights_t)(1 << 11))
/* The right to invoke `path_link` with the file descriptor as the
 * target directory. */
#define RIGHTS_PATH_LINK_TARGET ((rights_t)(1 << 12))
/* The right to invoke `path_open`. */
#define RIGHTS_PATH_OPEN ((rights_t)(1 << 13))
/* The right to invoke `fd_readdir`. */
#define RIGHTS_FD_READDIR ((rights_t)(1 << 14))
/* The right to invoke `path_readlink`. */
#define RIGHTS_PATH_READLINK ((rights_t)(1 << 15))
/* The right to invoke `path_rename` with the file descriptor as the source directory. */
#define RIGHTS_PATH_RENAME_SOURCE ((rights_t)(1 << 16))
/* The right to invoke `path_rename` with the file descriptor as the target directory. */
#define RIGHTS_PATH_RENAME_TARGET ((rights_t)(1 << 17))
/* The right to invoke `path_filestat_get`. */
#define RIGHTS_PATH_FILESTAT_GET ((rights_t)(1 << 18))
/* The right to change a file's size (there is no `path_filestat_set_size`).
 * If `path_open` is set, includes the right to invoke `path_open` with `oflags::trunc`. */
#define RIGHTS_PATH_FILESTAT_SET_SIZE ((rights_t)(1 << 19))
/* The right to invoke `path_filestat_set_times`. */
#define RIGHTS_PATH_FILESTAT_SET_TIMES ((rights_t)(1 << 20))
/* The right to invoke `fd_filestat_get`. */
#define RIGHTS_FD_FILESTAT_GET ((rights_t)(1 << 21))
/* The right to invoke `fd_filestat_set_size`. */
#define RIGHTS_FD_FILESTAT_SET_SIZE ((rights_t)(1 << 22))
/* The right to invoke `fd_filestat_set_times`. */
#define RIGHTS_FD_FILESTAT_SET_TIMES ((rights_t)(1 << 23))
/* The right to invoke `path_symlink`. */
#define RIGHTS_PATH_SYMLINK ((rights_t)(1 << 24))
/* The right to invoke `path_remove_directory`. */
#define RIGHTS_PATH_REMOVE_DIRECTORY ((rights_t)(1 << 25))
/* The right to invoke `path_unlink_file`. */
#define RIGHTS_PATH_UNLINK_FILE ((rights_t)(1 << 26))
/* If `rights::fd_read` is set, includes the right to invoke `poll_oneoff` to subscribe to `fd_read`.
 * If `rights::fd_write` is set, includes the right to invoke `poll_oneoff` to subscribe to `fd_write`. */
#define RIGHTS_POLL_FD_READWRITE ((rights_t)(1 << 27))
/* The right to invoke `sock_shutdown`. */
#define RIGHTS_SOCK_SHUTDOWN ((rights_t)(1 << 28))

/* A file descriptor handle. */
typedef int32_t fd_t;

/* A region of memory for scatter/gather reads. */
typedef struct iovec {
  uint8_t *buf; /* The address of the buffer to be filled. */
  size_t buf_len; /* The length of the buffer to be filled. */
} iovec_t;

/* A region of memory for scatter/gather writes. */
typedef struct ciovec {
  const uint8_t *buf; /* The address of the buffer to be written. */
  size_t buf_len; /* The length of the buffer to be written. */
} ciovec_t;

/* Relative offset within a file. */
typedef int64_t filedelta_t;

/* The position relative to which to set the offset of the file descriptor. */
typedef uint8_t whence_t;
#define WHENCE_SET 0 /* Seek relative to start-of-file. */
#define WHENCE_CUR 1 /* Seek relative to current position. */
#define WHENCE_END 2 /* Seek relative to end-of-file. */

/* A reference to the offset of a directory entry.
 * The value 0 signifies the start of the directory. */
typedef uint64_t dircookie_t;

/* The type for the `d_namlen` field of `dirent` struct. */
typedef uint32_t dirnamlen_t;

/* File serial number that is unique within its file system. */
typedef uint64_t inode_t;

/* The type of a file descriptor or file. */
typedef uint8_t filetype_t;

#define FILETYPE_UNKNOWN 0 /* The type of the file descriptor or file is unknown or is different from any of the other types specified. */
#define FILETYPE_BLOCK_DEVICE 1 /* The file descriptor or file refers to a block device inode. */
#define FILETYPE_CHARACTER_DEVICE 2 /* The file descriptor or file refers to a character device inode. */
#define FILETYPE_DIRECTORY 3 /* The file descriptor or file refers to a directory inode. */
#define FILETYPE_REGULAR_FILE 4 /* The file descriptor or file refers to a regular file inode. */
#define FILETYPE_SOCKET_DGRAM 5 /* The file descriptor or file refers to a datagram socket. */
#define FILETYPE_SOCKET_STREAM 6 /* The file descriptor or file refers to a byte-stream socket. */
#define FILETYPE_SYMBOLIC_LINK 7 /* The file refers to a symbolic link inode. */

/* A directory entry. */
typedef struct dirent_tag {
  dircookie_t d_next; /* The offset of the next directory entry stored in this directory. */
  inode_t d_ino; /* The serial number of the file referred to by this directory entry. */
  dirnamlen_t d_namlen; /* The length of the name of the directory entry. */
  filetype_t d_type; /* The type of the file referred to by this directory entry. */
} dirent_t;

/* File or memory access pattern advisory information. */
typedef uint8_t advice_t;

#define ADVICE_NORMAL 0 /* The application has no advice to give on its behavior with respect to the specified data. */
#define ADVICE_SEQUENTIAL 1 /* The application expects to access the specified data sequentially from lower offsets to higher offsets. */
#define ADVICE_RANDOM 2 /* The application expects to access the specified data in a random order. */
#define ADVICE_WILLNEED 3 /* The application expects to access the specified data in the near future. */
#define ADVICE_DONTNEED 4 /* The application expects that it will not access the specified data in the near future. */
#define ADVICE_NOREUSE 5 /* The application expects to access the specified data once and then not reuse it thereafter. */

/* File descriptor flags. */
typedef uint16_t fdflags_t;
/* Append mode: Data written to the file is always appended to the file's end. */
#define FDFLAGS_APPEND ((fdflags_t)(1 << 0))
/* Write according to synchronized I/O data integrity completion. Only the data stored in the file is synchronized. */
#define FDFLAGS_DSYNC ((fdflags_t)(1 << 1))
/* Non-blocking mode. */
#define FDFLAGS_NONBLOCK ((fdflags_t)(1 << 2))
/* Synchronized read I/O operations. */
#define FDFLAGS_RSYNC ((fdflags_t)(1 << 3))
/* Write according to synchronized I/O file integrity completion. In
 * addition to synchronizing the data stored in the file, the implementation
 * may also synchronously update the file's metadata. */
#define FDFLAGS_SYNC ((fdflags_t)(1 << 4))

/* File descriptor attributes. */
typedef struct fdstat {
  filetype_t fs_filetype; /* File type. */
  fdflags_t fs_flags; /* File descriptor flags. */
  rights_t fs_rights_base; /* Rights that apply to this file descriptor. */
  rights_t fs_rights_inheriting; /* Maximum set of rights that may be installed 
  on new file descriptors that are created through this file descriptor, e.g., through `path_open`. */
} fdstat_t;

/* Identifier for a device containing a file system. Can be used in combination
 * with `inode` to uniquely identify a file or directory in the filesystem. */
typedef uint64_t device_t;

/* Which file time attributes to adjust. */
typedef uint16_t fstflags_t;
/* Adjust the last data access timestamp to the value stored in `filestat::atim`. */
#define FSTFLAGS_ATIM ((fstflags_t)(1 << 0))
/* Adjust the last data access timestamp to the time of clock `clockid::realtime`. */
#define FSTFLAGS_ATIM_NOW ((fstflags_t)(1 << 1))
/* Adjust the last data modification timestamp to the value stored in `filestat::mtim`. */
#define FSTFLAGS_MTIM ((fstflags_t)(1 << 2))
/* Adjust the last data modification timestamp to the time of clock `clockid::realtime`. */
#define FSTFLAGS_MTIM_NOW ((fstflags_t)(1 << 3))
/* Flags determining the method of how paths are resolved. */
typedef uint32_t lookupflags_t;
/* As long as the resolved path corresponds to a symbolic link, it is expanded. */
#define LOOKUPFLAGS_SYMLINK_FOLLOW ((lookupflags_t)(1 << 0))
/* Open flags used by `path_open`. */
typedef uint16_t oflags_t;
/* Create file if it does not exist. */
#define OFLAGS_CREAT ((oflags_t)(1 << 0))
/* Fail if not a directory. */
#define OFLAGS_DIRECTORY ((oflags_t)(1 << 1))
/* Fail if file already exists. */
#define OFLAGS_EXCL ((oflags_t)(1 << 2))
/* Truncate file to size 0. */
#define OFLAGS_TRUNC ((oflags_t)(1 << 3))

/* Number of hard links to an inode. */
typedef uint64_t linkcount_t;

/* File attributes. */
typedef struct filestat {
  device_t dev; /* Device ID of device containing the file. */
  inode_t ino; /* File serial number. */
  filetype_t filetype; /* File type. */
  linkcount_t nlink; /* Number of hard links to the file. */
  filesize_t size; /* For regular files, the file size in bytes. For symlinks, the length in bytes of the pathname */
  timestamp_t atim; /* Last data access timestamp. */
  timestamp_t mtim; /* Last data modification timestamp. */
  timestamp_t ctim; /* Last file status change timestamp. */
} filestat_t;

/* User-provided value that may be attached to objects that is retained when
 * extracted from the implementation. */
typedef uint64_t userdata_t;

/* Type of a subscription to an event or its occurrence. */
typedef uint8_t eventtype_t;
/* The time value of clock `id` has
 * reached timestamp `timeout`. */
#define EVENTTYPE_CLOCK 0 
/* File descriptor `subscription_fd_readwrite::file_descriptor` has data
 * available for reading. This event always triggers for regular files. */
#define EVENTTYPE_FD_READ 1 
/* File descriptor `subscription_fd_readwrite::file_descriptor` has capacity
 * available for writing. This event always triggers for regular files. */
#define EVENTTYPE_FD_WRITE 2 

/* The state of the file descriptor subscribed to with `fd_read` or `fd_write`. */
typedef uint16_t eventrwflags_t;
/* The peer of this socket has closed or disconnected. */
#define EVENTRWFLAGS_FD_READWRITE_HANGUP ((eventrwflags_t)(1 << 0))
/* The contents of an `event` when type is `fd_read` or `fd_write`. */
typedef struct event_fd_readwrite {
  filesize_t nbytes; /* The number of bytes available for reading or writing. */
  eventrwflags_t flags; /* The state of the file descriptor. */
} event_fd_readwrite_t;

/* An event that occurred. */
typedef struct event {
  userdata_t userdata;/* User-provided value that got attached to `userdata`. */
  errno_t error; /* If non-zero, an error that occurred while processing the subscription request. */
  eventtype_t type; /* The type of event that occured */
  event_fd_readwrite_t fd_readwrite; /* The contents of the event, if it is an `fd_read` or
  `fd_write`. `clock` events ignore this field. */
} event_t;

/* Flags determining how to interpret the timestamp provided in `timeout`. */
typedef uint16_t subclockflags_t;
/* If set, treat the timestamp provided in `timeout` as an absolute timestamp of clock
 * `id`. If clear, treat the timestamp provided in `timeout` relative to the
 * current time value of clock `id`. */
#define SUBCLOCKFLAGS_SUBSCRIPTION_CLOCK_ABSTIME ((subclockflags_t)(1 << 0))
/* The contents of a `subscription` when type is `clock`. */
typedef struct subscription_clock {
  clockid_t id; /* The clock against which to compare the timestamp. */
  timestamp_t timeout; /* The absolute or relative timestamp. */
  timestamp_t precision;  /* The amount of time that the implementation may wait additionally
   * to coalesce with other events. */
  subclockflags_t flags; /* Flags specifying whether the timeout is absolute or relative */
} subscription_clock_t;

/* The contents of a `subscription` when type is type is `fd_read` or `fd_write`. */
typedef struct subscription_fd_readwrite {
  /* The file descriptor on which to wait for it to become ready for reading or writing. */
  fd_t file_descriptor;
} subscription_fd_readwrite_t;

/* The contents of a `subscription`. */
typedef union subscription_u_u_t {
  subscription_clock_t clock;
  subscription_fd_readwrite_t fd_read;
  subscription_fd_readwrite_t fd_write;
} subscription_u_u_t;
typedef struct subscription_u {
  uint8_t tag;
  subscription_u_u_t u;
} subscription_u_t;

/* Subscription to an event. */
typedef struct subscription {
  /* User-provided value that is attached to the subscription in the
   * implementation and returned through `event::userdata`. */
  userdata_t userdata;
  /* The type of the event to which to subscribe, and its contents */
  subscription_u_t u;
} subscription_t;

/* Exit code generated by a process when exiting. */
typedef uint32_t exitcode_t;

/* Signal condition. */
typedef uint8_t signal_t;
/* No signal. Note that POSIX has special semantics for `kill(pid, 0)`,
 * so this value is reserved. */
#define SIGNAL_NONE 0 
/* Hangup. Action: Terminates the process. */
#define SIGNAL_HUP 1 
/* Terminate interrupt signal. Action: Terminates the process. */
#define SIGNAL_INT 2 
/* Terminal quit signal. Action: Terminates the process. */
#define SIGNAL_QUIT 3 
/* Illegal instruction. Action: Terminates the process. */
#define SIGNAL_ILL 4 
/* Trace/breakpoint trap. Action: Terminates the process. */
#define SIGNAL_TRAP 5 
/* Process abort signal. Action: Terminates the process. */
#define SIGNAL_ABRT 6 
/* Access to an undefined portion of a memory object. Action: Terminates the process. */
#define SIGNAL_BUS 7 
/* Erroneous arithmetic operation. Action: Terminates the process. */
#define SIGNAL_FPE 8 
/* Kill. Action: Terminates the process. */
#define SIGNAL_KILL 9 
/* User-defined signal 1. Action: Terminates the process. */
#define SIGNAL_USR1 10 
/* Invalid memory reference. Action: Terminates the process. */
#define SIGNAL_SEGV 11 
/* User-defined signal 2. Action: Terminates the process. */
#define SIGNAL_USR2 12 
/* Write on a pipe with no one to read it. Action: Ignored. */
#define SIGNAL_PIPE 13 
/* Alarm clock. Action: Terminates the process. */
#define SIGNAL_ALRM 14 
/* Termination signal. Action: Terminates the process. */
#define SIGNAL_TERM 15 
/* Child process terminated, stopped, or continued. Action: Ignored. */
#define SIGNAL_CHLD 16 
/* Continue executing, if stopped. Action: Continues executing, if stopped. */
#define SIGNAL_CONT 17 
/* Stop executing. Action: Stops executing. */
#define SIGNAL_STOP 18 
/* Terminal stop signal. Action: Stops executing. */
#define SIGNAL_TSTP 19 
/* Background process attempting read. Action: Stops executing. */
#define SIGNAL_TTIN 20 
/* Background process attempting write. Action: Stops executing. */
#define SIGNAL_TTOU 21 
/* High bandwidth data is available at a socket. Action: Ignored. */
#define SIGNAL_URG 22 
/* CPU time limit exceeded. Action: Terminates the process. */
#define SIGNAL_XCPU 23 
/* File size limit exceeded. Action: Terminates the process. */
#define SIGNAL_XFSZ 24 
/* Virtual timer expired. Action: Terminates the process. */
#define SIGNAL_VTALRM 25 
/* Profiling timer expired. Action: Terminates the process. */
#define SIGNAL_PROF 26 
/* Window changed. Action: Ignored. */
#define SIGNAL_WINCH 27 
/* I/O possible. Action: Terminates the process. */
#define SIGNAL_POLL 28 
/* Power failure. Action: Terminates the process. */
#define SIGNAL_PWR 29 
/* Bad system call. Action: Terminates the process. */
#define SIGNAL_SYS 30 

/* Flags provided to `sock_recv`. */
typedef uint16_t riflags_t;
/* Returns the message without removing it from the socket's receive queue. */
#define RIFLAGS_RECV_PEEK ((riflags_t)(1 << 0))
/* On byte-stream sockets, block until the full amount of data can be returned. */
#define RIFLAGS_RECV_WAITALL ((riflags_t)(1 << 1))
/* Flags returned by `sock_recv`. */
typedef uint16_t roflags_t;
/* Returned by `sock_recv`: Message data has been truncated. */
#define ROFLAGS_RECV_DATA_TRUNCATED ((roflags_t)(1 << 0))
/* Flags provided to `sock_send`. As there are currently no flags
 * defined, it must be set to zero. */
typedef uint16_t siflags_t;

/* Which channels on a socket to shut down. */
typedef uint8_t sdflags_t;
/* Disables further receive operations. */
#define SDFLAGS_RD ((sdflags_t)(1 << 0))
/* Disables further send operations. */
#define SDFLAGS_WR ((sdflags_t)(1 << 1))
/* Identifiers for preopened capabilities. */
typedef uint8_t preopentype_t;
/* A pre-opened directory. */
#define PREOPENTYPE_DIR 0 

/* The contents of a $prestat when type is `preopentype::dir`. */
typedef struct prestat_dir {
  /* The length of the directory name for use with `fd_prestat_dir_name`. */
  size_t pr_name_len;
} prestat_dir_t;

/* Information about a pre-opened capability. */
typedef union prestat_u_t {
  prestat_dir_t dir;
} prestat_u_t;
typedef struct prestat {
  uint8_t tag;
  prestat_u_t u;
} prestat_t;

/* Read command-line argument data.
 * The size of the array should match that returned by `args_sizes_get` */
extern errno_t args_get(
  uint8_t **argv,
  uint8_t *argv_buf
);

/* Return command-line argument data sizes.
 * @return
 * Returns the number of arguments and the size of the argument string
 * data, or an error. */
extern errno_t args_sizes_get(
  size_t *retptr0,
  size_t *retptr1
);

/* Read environment variable data.
 * The sizes of the buffers should match that returned by `environ_sizes_get`. */
extern errno_t environ_get(
  uint8_t **environ,
  uint8_t *environ_buf
);

/* Return environment variable data sizes.
 * @return
 * Returns the number of environment variable arguments and the size of the
 * environment variable data. */
extern errno_t environ_sizes_get(
  size_t *retptr0,
  size_t *retptr1
);

/* Return the resolution of a clock.
 * Implementations are required to provide a non-zero value for supported clocks. For unsupported clocks,
 * return `errno::inval`.
 * Note: This is similar to `clock_getres` in POSIX.
 * @return
 * The resolution of the clock, or an error if one happened. */
extern errno_t clock_res_get(
  /* The clock for which to return the resolution. */
  clockid_t id,
  timestamp_t *retptr0
);

/* Return the time value of a clock.
 * Note: This is similar to `clock_gettime` in POSIX.
 * @return
 * The time value of the clock. */
extern errno_t clock_time_get(
  /* The clock for which to return the time. */
  clockid_t id,
  /* The maximum lag (exclusive) that the returned time value may have, compared to its actual value. */
  timestamp_t precision,
  timestamp_t *retptr0
);

/* Provide file advisory information on a file descriptor.
 * Note: This is similar to `posix_fadvise` in POSIX. */
extern errno_t fd_advise(
  fd_t fd,
  /* The offset within the file to which the advisory applies. */
  filesize_t offset,
  /* The length of the region to which the advisory applies. */
  filesize_t len,
  /* The advice. */
  advice_t advice
);

/* Force the allocation of space in a file.
 * Note: This is similar to `posix_fallocate` in POSIX. */
extern errno_t fd_allocate(
  fd_t fd,
  /* The offset at which to start the allocation. */
  filesize_t offset,
  /* The length of the area that is allocated. */
  filesize_t len
);

/* Close a file descriptor.
 * Note: This is similar to `close` in POSIX. */
extern errno_t fd_close(
  fd_t fd
);

/* Synchronize the data of a file to disk.
 * Note: This is similar to `fdatasync` in POSIX. */
extern errno_t fd_datasync(
  fd_t fd
);

/* Get the attributes of a file descriptor.
 * Note: This returns similar flags to `fsync(fd, F_GETFL)` in POSIX, as well as additional fields.
 * @return
 * The buffer where the file descriptor's attributes are stored. */
extern errno_t fd_fdstat_get(
  fd_t fd,
  fdstat_t *retptr0
);

/* Adjust the flags associated with a file descriptor.
 * Note: This is similar to `fcntl(fd, F_SETFL, flags)` in POSIX. */
extern errno_t fd_fdstat_set_flags(
  fd_t fd,
  /* The desired values of the file descriptor flags. */
  fdflags_t flags
);

/* Adjust the rights associated with a file descriptor.
 * This can only be used to remove rights, and returns `errno::notcapable` if called in a way that would attempt to add rights */
extern errno_t fd_fdstat_set_rights(
  fd_t fd,
  /* The desired rights of the file descriptor. */
  rights_t fs_rights_base,
  rights_t fs_rights_inheriting
);

/* Return the attributes of an open file.
 * @return
 * The buffer where the file's attributes are stored. */
extern errno_t fd_filestat_get(
  fd_t fd,
  filestat_t *retptr0
);

/* Adjust the size of an open file. If this increases the file's size, the extra bytes are filled with zeros.
 * Note: This is similar to `ftruncate` in POSIX. */
extern errno_t fd_filestat_set_size(
  fd_t fd,
  /* The desired file size. */
  filesize_t size
);

/* Adjust the timestamps of an open file or directory.
 * Note: This is similar to `futimens` in POSIX. */
extern errno_t fd_filestat_set_times(
  fd_t fd,
  /* The desired values of the data access timestamp. */
  timestamp_t atim,
  /* The desired values of the data modification timestamp. */
  timestamp_t mtim,
  /* A bitmask indicating which timestamps to adjust. */
  fstflags_t fst_flags
);

/* Read from a file descriptor, without using and updating the file descriptor's offset.
 * Note: This is similar to `preadv` in POSIX.
 * @return
 * The number of bytes read. */
extern errno_t fd_pread(
  fd_t fd,
  /* List of scatter/gather vectors in which to store data. */
  const iovec_t *iovs,
  /* The length of the array pointed to by `iovs`. */
  size_t iovs_len,
  /* The offset within the file at which to read. */
  filesize_t offset,
  size_t *retptr0
);

/* Return a description of the given preopened file descriptor.
 * @return
 * The buffer where the description is stored. */
extern errno_t fd_prestat_get(
  fd_t fd,
  prestat_t *retptr0
);

/* Return a description of the given preopened file descriptor. */
extern errno_t fd_prestat_dir_name(
  fd_t fd,
  /* A buffer into which to write the preopened directory name. */
  uint8_t * path,
  size_t path_len
);

/* Write to a file descriptor, without using and updating the file descriptor's offset.
 * Note: This is similar to `pwritev` in POSIX.
 * @return
 * The number of bytes written. */
extern errno_t fd_pwrite(
  fd_t fd,
  /* List of scatter/gather vectors from which to retrieve data. */
  const ciovec_t *iovs,
  /* The length of the array pointed to by `iovs`. */
  size_t iovs_len,
  /* The offset within the file at which to write. */
  filesize_t offset,
  size_t *retptr0
);

/* Read from a file descriptor.
 * Note: This is similar to `readv` in POSIX.
 * @return
 * The number of bytes read. */
extern errno_t fd_read(
  fd_t fd,
  /* List of scatter/gather vectors to which to store data. */
  const iovec_t *iovs,
  /* The length of the array pointed to by `iovs`. */
  size_t iovs_len,
  size_t *retptr0
);

/* Read directory entries from a directory.
 * When successful, the contents of the output buffer consist of a sequence of
 * directory entries. Each directory entry consists of a `dirent` object,
 * followed by `dirent::d_namlen` bytes holding the name of the directory
 * entry.
 * This function fills the output buffer as much as possible, potentially
 * truncating the last directory entry. This allows the caller to grow its
 * read buffer size in case it's too small to fit a single large directory
 * entry, or skip the oversized directory entry.
 * @return
 * The number of bytes stored in the read buffer. If less than the size of the read 
 * buffer, the end of the directory has been reached. */
extern errno_t fd_readdir(
  fd_t fd,
  /* The buffer where directory entries are stored */
  uint8_t *buf,
  size_t buf_len,
  /* The location within the directory to start reading */
  dircookie_t cookie,
  size_t *retptr0
);

/* Atomically replace a file descriptor by renumbering another file descriptor.
 * Due to the strong focus on thread safety, this environment does not provide
 * a mechanism to duplicate or renumber a file descriptor to an arbitrary
 * number, like `dup2()`. This would be prone to race conditions, as an actual
 * file descriptor with the same number could be allocated by a different
 * thread at the same time.
 * This function provides a way to atomically renumber file descriptors, which
 * would disappear if `dup2()` were to be removed entirely. */
extern errno_t fd_renumber(
  fd_t fd,
  /* The file descriptor to overwrite. */
  fd_t to
);

/* Move the offset of a file descriptor.
 * Note: This is similar to `lseek` in POSIX.
 * @return
 * The new offset of the file descriptor, relative to the start of the file. */
extern errno_t fd_seek(
  fd_t fd,
  /* The number of bytes to move. */
  filedelta_t offset,
  /* The base from which the offset is relative. */
  whence_t whence,
  filesize_t *retptr0
);

/* Synchronize the data and metadata of a file to disk.
 * Note: This is similar to `fsync` in POSIX. */
extern errno_t fd_sync(
  fd_t fd
);

/* Return the current offset of a file descriptor.
 * Note: This is similar to `lseek(fd, 0, SEEK_CUR)` in POSIX.
 * @return
 * The current offset of the file descriptor, relative to the start of the file. */
extern errno_t fd_tell(
  fd_t fd,
  filesize_t *retptr0
);

/* Write to a file descriptor.
 * Note: This is similar to `writev` in POSIX. */
extern errno_t fd_write(
  fd_t fd,
  /* List of scatter/gather vectors from which to retrieve data. */
  const ciovec_t *iovs,
  /* The length of the array pointed to by `iovs`. */
  size_t iovs_len,
  size_t *retptr0
);

/* Create a directory.
 * Note: This is similar to `mkdirat` in POSIX. */
extern errno_t path_create_directory(
  fd_t fd,
  /* The path at which to create the directory. */
  const char *path,
  size_t path_len /* e.g. strlen(path) */
);

/* Return the attributes of a file or directory.
 * Note: This is similar to `stat` in POSIX.
 * @return
 * The buffer where the file's attributes are stored. */
extern errno_t path_filestat_get(
  fd_t fd,
  /* Flags determining the method of how the path is resolved. */
  lookupflags_t flags,
  /* The path of the file or directory to inspect. */
  const char *path,
  size_t path_len, /* e.g. strlen(path) */
  filestat_t *retptr0
);

/* Adjust the timestamps of a file or directory.
 * Note: This is similar to `utimensat` in POSIX. */
extern errno_t path_filestat_set_times(
  fd_t fd,
  /* Flags determining the method of how the path is resolved. */
  lookupflags_t flags,
  /* The path of the file or directory to operate on. */
  const char *path,
  size_t path_len, /* e.g. strlen(path) */
  /* The desired values of the data access timestamp. */
  timestamp_t atim,
  /* The desired values of the data modification timestamp. */
  timestamp_t mtim,
  /* A bitmask indicating which timestamps to adjust. */
  fstflags_t fst_flags
);

/* Create a hard link.
 * Note: This is similar to `linkat` in POSIX. */
extern errno_t path_link(
  fd_t old_fd,
  /* Flags determining the method of how the path is resolved. */
  lookupflags_t old_flags,
  /* The source path from which to link. */
  const char *old_path,
  size_t old_path_len, /* e.g. strlen(old_path) */
  /* The working directory at which the resolution of the new path starts. */
  fd_t new_fd,
  /* The destination path at which to create the hard link. */
  const char *new_path,
  size_t new_path_len /* e.g. strlen(new_path) */
);

/* Open a file or directory.
 * The returned file descriptor is not guaranteed to be the lowest-numbered
 * file descriptor not currently open; it is randomized to prevent
 * applications from depending on making assumptions about indexes, since this
 * is error-prone in multi-threaded contexts. The returned file descriptor is
 * guaranteed to be less than 2**31.
 * Note: This is similar to `openat` in POSIX.
 * @return
 * The file descriptor of the file that has been opened. */
extern errno_t path_open(
  fd_t fd,
  /* Flags determining the method of how the path is resolved. */
  lookupflags_t dirflags,
  /* The relative path of the file or directory to open, relative to the
   * `path_open::fd` directory. */
  const char *path,
  size_t path_len, /* e.g. strlen(path) */
  /* The method by which to open the file. */
  oflags_t oflags,
  /* The initial rights of the newly created file descriptor. The
   * implementation is allowed to return a file descriptor with fewer rights
   * than specified, if and only if those rights do not apply to the type of
   * file being opened.
   * The *base* rights are rights that will apply to operations using the file
   * descriptor itself, while the *inheriting* rights are rights that apply to
   * file descriptors derived from it. */
  rights_t fs_rights_base,
  rights_t fs_rights_inheriting,
  fdflags_t fdflags,
  fd_t *retptr0
);

/* Read the contents of a symbolic link.
 * Note: This is similar to `readlinkat` in POSIX.
 * @return
 * The number of bytes placed in the buffer. */
extern errno_t path_readlink(
  fd_t fd,
  /* The path of the symbolic link from which to read. */
  const char *path,
  size_t path_len, /* e.g. strlen(path) */
  /* The buffer to which to write the contents of the symbolic link. */
  uint8_t *buf,
  size_t buf_len,
  size_t *retptr0
);

/* Remove a directory.
 * Return `errno::notempty` if the directory is not empty.
 * Note: This is similar to `unlinkat(fd, path, AT_REMOVEDIR)` in POSIX. */
extern errno_t path_remove_directory(
  fd_t fd,
  /* The path to a directory to remove. */
  const char *path,
  size_t path_len /* e.g. strlen(path) */
);

/* Rename a file or directory.
 * Note: This is similar to `renameat` in POSIX. */
extern errno_t path_rename(
  fd_t fd,
  /* The source path of the file or directory to rename. */
  const char *old_path,
  size_t old_path_len, /* e.g. strlen(old_path) */
  /* The working directory at which the resolution of the new path starts. */
  fd_t new_fd,
  /* The destination path to which to rename the file or directory. */
  const char *new_path,
  size_t new_path_len /* e.g. strlen(new_path) */  
);

/* Create a symbolic link.
 * Note: This is similar to `symlinkat` in POSIX. */
extern errno_t path_symlink(
  /* The contents of the symbolic link. */
  const char *old_path,
  size_t old_path_len, /* e.g. strlen(old_path) */
  fd_t fd,
  /* The destination path at which to create the symbolic link. */
  const char *new_path,
  size_t new_path_len /* e.g. strlen(new_path) */
);

/* Unlink a file.
 * Return `errno::isdir` if the path refers to a directory.
 * Note: This is similar to `unlinkat(fd, path, 0)` in POSIX. */
extern errno_t path_unlink_file(
  fd_t fd,
  /* The path to a file to unlink. */
  const char *path,
  size_t path_len /* e.g. strlen(path) */
);

/* Concurrently poll for the occurrence of a set of events.
 * @return
 * The number of events stored. */
extern errno_t poll_oneoff(
  /* The events to which to subscribe. */
  const subscription_t * in,
  /* The events that have occurred. */
  event_t * out,
  /* Both the number of subscriptions and events. */
  size_t nsubscriptions,
  size_t *retptr0
);

/* Terminate the process normally. An exit code of 0 indicates successful
 * termination of the program. The meanings of other values is dependent on
 * the environment. */
extern void proc_exit(
  /* The exit code returned by the process. */
  exitcode_t rval
);

/* Send a signal to the process of the calling thread.
 * Note: This is similar to `raise` in POSIX. */
extern errno_t proc_raise(
  /* The signal condition to trigger. */
  signal_t sig
);

/* Temporarily yield execution of the calling thread.
 * Note: This is similar to `sched_yield` in POSIX. */
extern errno_t sched_yield(
  void
);

/* Write high-quality random data into a buffer.
 * This function blocks when the implementation is unable to immediately
 * provide sufficient high-quality random data.
 * This function may execute slowly, so when large mounts of random data are
 * required, it's advisable to use this function to seed a pseudo-random
 * number generator, rather than to provide the random data directly. */
extern errno_t random_get(
  /* The buffer to fill with random data. */
  uint8_t *buf,
  size_t buf_len
);

/* Receive a message from a socket.
 * Note: This is similar to `recv` in POSIX, though it also supports reading
 * the data into multiple buffers in the manner of `readv`.
 * @return
 * Number of bytes stored in ri_data and message flags. */
extern errno_t sock_recv(
  fd_t fd,
  /* List of scatter/gather vectors to which to store data. */
  const iovec_t *ri_data,
  /* The length of the array pointed to by `ri_data`. */
  size_t ri_data_len,
  /* Message flags. */
  riflags_t ri_flags,
  size_t *retptr0,
  roflags_t *retptr1
);

/* Send a message on a socket.
 * Note: This is similar to `send` in POSIX, though it also supports writing
 * the data from multiple buffers in the manner of `writev`.
 * @return
 * Number of bytes transmitted. */
extern errno_t sock_send(
  fd_t fd,
  /* List of scatter/gather vectors to which to retrieve data */
  const ciovec_t *si_data,
  /* The length of the array pointed to by `si_data`. */
  size_t si_data_len,
  /* Message flags. */
  siflags_t si_flags,
  size_t *retptr0
);

/* Shut down socket send and receive channels.
 * Note: This is similar to `shutdown` in POSIX. */
extern errno_t sock_shutdown(
  fd_t fd,
  /* Which channels on the socket to shut down. */
  sdflags_t how
);


/* make sure sizes, offsets, and aligns are as expected by WASI */
static_assert(alignof(advice_t) == 1, "witx calculated align");
static_assert(alignof(ciovec_t) == 4, "witx calculated align");
static_assert(alignof(clockid_t) == 4, "witx calculated align");
static_assert(alignof(device_t) == 8, "witx calculated align");
static_assert(alignof(dircookie_t) == 8, "witx calculated align");
static_assert(alignof(dirent_t) == 8, "witx calculated align");
static_assert(alignof(dirnamlen_t) == 4, "witx calculated align");
static_assert(alignof(errno_t) == 2, "witx calculated align");
static_assert(alignof(event_fd_readwrite_t) == 8, "witx calculated align");
static_assert(alignof(event_t) == 8, "witx calculated align");
static_assert(alignof(eventtype_t) == 1, "witx calculated align");
static_assert(alignof(exitcode_t) == 4, "witx calculated align");
static_assert(alignof(fd_t) == 4, "witx calculated align");
static_assert(alignof(fdstat_t) == 8, "witx calculated align");
static_assert(alignof(filedelta_t) == 8, "witx calculated align");
static_assert(alignof(filesize_t) == 8, "witx calculated align");
static_assert(alignof(filestat_t) == 8, "witx calculated align");
static_assert(alignof(filetype_t) == 1, "witx calculated align");
static_assert(alignof(inode_t) == 8, "witx calculated align");
static_assert(alignof(iovec_t) == 4, "witx calculated align");
static_assert(alignof(linkcount_t) == 8, "witx calculated align");
static_assert(alignof(preopentype_t) == 1, "witx calculated align");
static_assert(alignof(prestat_dir_t) == 4, "witx calculated align");
static_assert(alignof(prestat_t) == 4, "witx calculated align");
static_assert(alignof(siflags_t) == 2, "witx calculated align");
static_assert(alignof(signal_t) == 1, "witx calculated align");
static_assert(alignof(size_t) == 4, "witx calculated align");
static_assert(alignof(subscription_clock_t) == 8, "witx calculated align");
static_assert(alignof(subscription_fd_readwrite_t) == 4, "witx calculated align");
static_assert(alignof(subscription_t) == 8, "witx calculated align");
static_assert(alignof(subscription_u_t) == 8, "witx calculated align");
static_assert(alignof(timestamp_t) == 8, "witx calculated align");
static_assert(alignof(userdata_t) == 8, "witx calculated align");
static_assert(alignof(whence_t) == 1, "witx calculated align");
static_assert(alignof(int16_t) == 2, "non-wasi data layout");
static_assert(alignof(int32_t) == 4, "non-wasi data layout");
static_assert(alignof(int64_t) == 8, "non-wasi data layout");
static_assert(alignof(int8_t) == 1, "non-wasi data layout");
static_assert(alignof(uint16_t) == 2, "non-wasi data layout");
static_assert(alignof(uint32_t) == 4, "non-wasi data layout");
static_assert(alignof(uint64_t) == 8, "non-wasi data layout");
static_assert(alignof(uint8_t) == 1, "non-wasi data layout");
static_assert(alignof(void*) == 4, "non-wasi data layout");
static_assert(offsetof(ciovec_t, buf) == 0, "witx calculated offset");
static_assert(offsetof(ciovec_t, buf_len) == 4, "witx calculated offset");
static_assert(offsetof(dirent_t, d_ino) == 8, "witx calculated offset");
static_assert(offsetof(dirent_t, d_namlen) == 16, "witx calculated offset");
static_assert(offsetof(dirent_t, d_next) == 0, "witx calculated offset");
static_assert(offsetof(dirent_t, d_type) == 20, "witx calculated offset");
static_assert(offsetof(event_fd_readwrite_t, flags) == 8, "witx calculated offset");
static_assert(offsetof(event_fd_readwrite_t, nbytes) == 0, "witx calculated offset");
static_assert(offsetof(event_t, error) == 8, "witx calculated offset");
static_assert(offsetof(event_t, fd_readwrite) == 16, "witx calculated offset");
static_assert(offsetof(event_t, type) == 10, "witx calculated offset");
static_assert(offsetof(event_t, userdata) == 0, "witx calculated offset");
static_assert(offsetof(fdstat_t, fs_filetype) == 0, "witx calculated offset");
static_assert(offsetof(fdstat_t, fs_flags) == 2, "witx calculated offset");
static_assert(offsetof(fdstat_t, fs_rights_base) == 8, "witx calculated offset");
static_assert(offsetof(fdstat_t, fs_rights_inheriting) == 16, "witx calculated offset");
static_assert(offsetof(filestat_t, atim) == 40, "witx calculated offset");
static_assert(offsetof(filestat_t, ctim) == 56, "witx calculated offset");
static_assert(offsetof(filestat_t, dev) == 0, "witx calculated offset");
static_assert(offsetof(filestat_t, filetype) == 16, "witx calculated offset");
static_assert(offsetof(filestat_t, ino) == 8, "witx calculated offset");
static_assert(offsetof(filestat_t, mtim) == 48, "witx calculated offset");
static_assert(offsetof(filestat_t, nlink) == 24, "witx calculated offset");
static_assert(offsetof(filestat_t, size) == 32, "witx calculated offset");
static_assert(offsetof(iovec_t, buf) == 0, "witx calculated offset");
static_assert(offsetof(iovec_t, buf_len) == 4, "witx calculated offset");
static_assert(offsetof(prestat_dir_t, pr_name_len) == 0, "witx calculated offset");
static_assert(offsetof(subscription_clock_t, flags) == 24, "witx calculated offset");
static_assert(offsetof(subscription_clock_t, id) == 0, "witx calculated offset");
static_assert(offsetof(subscription_clock_t, precision) == 16, "witx calculated offset");
static_assert(offsetof(subscription_clock_t, timeout) == 8, "witx calculated offset");
static_assert(offsetof(subscription_fd_readwrite_t, file_descriptor) == 0, "witx calculated offset");
static_assert(offsetof(subscription_t, u) == 8, "witx calculated offset");
static_assert(offsetof(subscription_t, userdata) == 0, "witx calculated offset");
static_assert(sizeof(advice_t) == 1, "witx calculated size");
static_assert(sizeof(ciovec_t) == 8, "witx calculated size");
static_assert(sizeof(clockid_t) == 4, "witx calculated size");
static_assert(sizeof(device_t) == 8, "witx calculated size");
static_assert(sizeof(dircookie_t) == 8, "witx calculated size");
static_assert(sizeof(dirent_t) == 24, "witx calculated size");
static_assert(sizeof(dirnamlen_t) == 4, "witx calculated size");
static_assert(sizeof(errno_t) == 2, "witx calculated size");
static_assert(sizeof(event_fd_readwrite_t) == 16, "witx calculated size");
static_assert(sizeof(event_t) == 32, "witx calculated size");
static_assert(sizeof(eventtype_t) == 1, "witx calculated size");
static_assert(sizeof(exitcode_t) == 4, "witx calculated size");
static_assert(sizeof(fd_t) == 4, "witx calculated size");
static_assert(sizeof(fdstat_t) == 24, "witx calculated size");
static_assert(sizeof(filedelta_t) == 8, "witx calculated size");
static_assert(sizeof(filesize_t) == 8, "witx calculated size");
static_assert(sizeof(filestat_t) == 64, "witx calculated size");
static_assert(sizeof(filetype_t) == 1, "witx calculated size");
static_assert(sizeof(inode_t) == 8, "witx calculated size");
static_assert(sizeof(iovec_t) == 8, "witx calculated size");
static_assert(sizeof(linkcount_t) == 8, "witx calculated size");
static_assert(sizeof(preopentype_t) == 1, "witx calculated size");
static_assert(sizeof(prestat_dir_t) == 4, "witx calculated size");
static_assert(sizeof(prestat_t) == 8, "witx calculated size");
static_assert(sizeof(siflags_t) == 2, "witx calculated size");
static_assert(sizeof(signal_t) == 1, "witx calculated size");
static_assert(sizeof(size_t) == 4, "witx calculated size");
static_assert(sizeof(subscription_clock_t) == 32, "witx calculated size");
static_assert(sizeof(subscription_fd_readwrite_t) == 4, "witx calculated size");
static_assert(sizeof(subscription_t) == 48, "witx calculated size");
static_assert(sizeof(subscription_u_t) == 40, "witx calculated size");
static_assert(sizeof(timestamp_t) == 8, "witx calculated size");
static_assert(sizeof(userdata_t) == 8, "witx calculated size");
static_assert(sizeof(whence_t) == 1, "witx calculated size");
