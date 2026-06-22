/* dtc.h - eindeutige Fehler-Marker (DTC) ueber Datei-Basis + Zeilennummer.
   In jeder .c VOR dem Include DTCBASE definieren, z.B.:
       #define DTCBASE 10000
       #include "dtc.h"
   Dann erzeugt jeder puterror()/puterrstr()/dtcerr() automatisch einen
   eindeutigen Code DTC<DTCBASE+__LINE__> - keine Dubletten moeglich. */
#ifndef DTC_H_
#define DTC_H_
#ifndef DTCBASE
#define DTCBASE 0
#endif
#undef  puterror
#undef  puterrstr
#undef  dtcerr
#define puterror(e,v)  puterror_dtc((e),(v),(DTCBASE)+__LINE__)
#define puterrstr(ln)  puterrstr_dtc((ln),(DTCBASE)+__LINE__)
#define dtcerr(msg)    dctext((msg),(DTCBASE)+__LINE__)
#endif
