/* Errors */

#include <errno.h>
#include <wasi/api.h>

int errno;

/* used by perror
const char *_emsg[77] = {
  "No error occurred",
  "Argument list too long",
  "Permission denied",
  "Address in use",
  "Address not available",
  "Address family not supported",
  "Resource unavailable, or operation would block",
  "Connection already in progress",
  "Bad file descriptor",
  "Bad message",
  "Device or resource busy",
  "Operation canceled",
  "No child processes",
  "Connection aborted",
  "Connection refused",
  "Connection reset",
  "Resource deadlock would occur",
  "Destination address required",
  "Mathematics argument out of domain of function",
  "Reserved",
  "File exists",
  "Bad address",
  "File too large",
  "Host is unreachable",
  "Identifier removed",
  "Illegal byte sequence",
  "Operation in progress",
  "Interrupted function",
  "Invalid argument",
  "I/O error",
  "Socket is connected",
  "Is a directory",
  "Too many levels of symbolic links",
  "File descriptor value too large",
  "Too many links",
  "Message too large",
  "Reserved",
  "Filename too long",
  "Network is down",
  "Connection aborted by network",
  "Network unreachable",
  "Too many files open in system",
  "No buffer space available",
  "No such device",
  "No such file or directory",
  "Executable file format error",
  "No locks available",
  "Reserved",
  "Not enough space",
  "No message of the desired type",
  "Protocol not available",
  "No space left on device",
  "Function not supported",
  "The socket is not connected",
  "Not a directory or a symbolic link to a directory",
  "Directory not empty",
  "State not recoverable",
  "Not a socket",
  "Not supported, or operation not supported on socket",
  "Inappropriate I/O control operation",
  "No such device or address",
  "Value too large to be stored in data type",
  "Previous owner died",
  "Operation not permitted",
  "Broken pipe",
  "Protocol error",
  "Protocol not supported",
  "Protocol wrong type for socket",
  "Result too large",
  "Read-only file system",
  "Invalid seek",
  "No such process",
  "Reserved",
  "Connection timed out",
  "Text file busy",
  "Cross-device link",
  "Extension: Capabilities insufficient"
}; */

static_assert(ESUCCESS == ERRNO_SUCCESS);
static_assert(E2BIG == ERRNO_2BIG);
static_assert(EACCES == ERRNO_ACCES);
static_assert(EADDRINUSE == ERRNO_ADDRINUSE);
static_assert(EADDRNOTAVAIL == ERRNO_ADDRNOTAVAIL);
static_assert(EAFNOSUPPORT == ERRNO_AFNOSUPPORT);
static_assert(EAGAIN == ERRNO_AGAIN);
static_assert(EALREADY == ERRNO_ALREADY);
static_assert(EBADF == ERRNO_BADF);
static_assert(EBADMSG == ERRNO_BADMSG);
static_assert(EBUSY == ERRNO_BUSY);
static_assert(ECANCELED == ERRNO_CANCELED);
static_assert(ECHILD == ERRNO_CHILD);
static_assert(ECONNABORTED == ERRNO_CONNABORTED);
static_assert(ECONNREFUSED == ERRNO_CONNREFUSED);
static_assert(ECONNRESET == ERRNO_CONNRESET);
static_assert(EDEADLK == ERRNO_DEADLK);
static_assert(EDESTADDRREQ == ERRNO_DESTADDRREQ);
static_assert(EDOM == ERRNO_DOM);
static_assert(EDQUOT == ERRNO_DQUOT);
static_assert(EEXIST == ERRNO_EXIST);
static_assert(EFAULT == ERRNO_FAULT);
static_assert(EFBIG == ERRNO_FBIG);
static_assert(EHOSTUNREACH == ERRNO_HOSTUNREACH);
static_assert(EIDRM == ERRNO_IDRM);
static_assert(EILSEQ == ERRNO_ILSEQ);
static_assert(EINPROGRESS == ERRNO_INPROGRESS);
static_assert(EINTR == ERRNO_INTR);
static_assert(EINVAL == ERRNO_INVAL);
static_assert(EIO == ERRNO_IO);
static_assert(EISCONN == ERRNO_ISCONN);
static_assert(EISDIR == ERRNO_ISDIR);
static_assert(ELOOP == ERRNO_LOOP);
static_assert(EMFILE == ERRNO_MFILE);
static_assert(EMLINK == ERRNO_MLINK);
static_assert(EMSGSIZE == ERRNO_MSGSIZE);
static_assert(EMULTIHOP == ERRNO_MULTIHOP);
static_assert(ENAMETOOLONG == ERRNO_NAMETOOLONG);
static_assert(ENETDOWN == ERRNO_NETDOWN);
static_assert(ENETRESET == ERRNO_NETRESET);
static_assert(ENETUNREACH == ERRNO_NETUNREACH);
static_assert(ENFILE == ERRNO_NFILE);
static_assert(ENOBUFS == ERRNO_NOBUFS);
static_assert(ENODEV == ERRNO_NODEV);
static_assert(ENOENT == ERRNO_NOENT);
static_assert(ENOEXEC == ERRNO_NOEXEC);
static_assert(ENOLCK == ERRNO_NOLCK);
static_assert(ENOLINK == ERRNO_NOLINK);
static_assert(ENOMEM == ERRNO_NOMEM);
static_assert(ENOMSG == ERRNO_NOMSG);
static_assert(ENOPROTOOPT == ERRNO_NOPROTOOPT);
static_assert(ENOSPC == ERRNO_NOSPC);
static_assert(ENOSYS == ERRNO_NOSYS);
static_assert(ENOTCONN == ERRNO_NOTCONN);
static_assert(ENOTDIR == ERRNO_NOTDIR);
static_assert(ENOTEMPTY == ERRNO_NOTEMPTY);
static_assert(ENOTRECOVERABLE == ERRNO_NOTRECOVERABLE);
static_assert(ENOTSOCK == ERRNO_NOTSOCK);
static_assert(ENOTSUP == ERRNO_NOTSUP);
static_assert(ENOTTY == ERRNO_NOTTY);
static_assert(ENXIO == ERRNO_NXIO);
static_assert(EOVERFLOW == ERRNO_OVERFLOW);
static_assert(EOWNERDEAD == ERRNO_OWNERDEAD);
static_assert(EPERM == ERRNO_PERM);
static_assert(EPIPE == ERRNO_PIPE);
static_assert(EPROTO == ERRNO_PROTO);
static_assert(EPROTONOSUPPORT == ERRNO_PROTONOSUPPORT);
static_assert(EPROTOTYPE == ERRNO_PROTOTYPE);
static_assert(ERANGE == ERRNO_RANGE);
static_assert(EROFS == ERRNO_ROFS);
static_assert(ESPIPE == ERRNO_SPIPE);
static_assert(ESRCH == ERRNO_SRCH);
static_assert(ESTALE == ERRNO_STALE);
static_assert(ETIMEDOUT == ERRNO_TIMEDOUT);
static_assert(ETXTBSY == ERRNO_TXTBSY);
static_assert(EXDEV == ERRNO_XDEV);
static_assert(ENOTCAPABLE == ERRNO_NOTCAPABLE);
