/* OpenOrbis net_http sample calls sceSslTerm(context) from C. */
#include <stddef.h>

#include <orbis/Ssl.h>
void dmSslTerm(int context){sceSslTerm(context);}
/* Prototypes verified against shadPS4 src/core/libraries/network/http.h.
 * OpenOrbis exports these symbols but leaves their C declarations unspecified. */
extern int sceHttpSetAutoRedirect(int id,int enabled);
extern int sceHttpSetRecvTimeOut(int id,unsigned int usec);
int dmHttpConfigure(int id,unsigned int timeout){int rc=sceHttpSetAutoRedirect(id,0);return rc<0?rc:sceHttpSetRecvTimeOut(id,timeout);}
