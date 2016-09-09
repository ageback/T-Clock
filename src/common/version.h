#ifndef AUTOVERSION_H
#define AUTOVERSION_H
/* Note: to use integer defines as strings, use STR(), eg. STR(VER_REVISION) */
/**** Version ****/
#	define VER_MAJOR 2
#	define VER_MINOR 4
#	define VER_BUILD 2
	/** status values: 0=Alpha, 1=Beta, 2=RC, 3=Release, 4=Maintenance */
#	define VER_STATUS 1
#	define VER_STATUS_FULL "Beta"
#	define VER_STATUS_SHORT "b"
#	define VER_STATUS_GREEK "Î²"
#	define VER_REVISION 459
#	define VER_FULL "2.4.2 Beta"
#	define VER_SHORT "2.4b2"
#	define VER_SHORT_DOTS "2.4.2"
#	define VER_SHORT_GREEK "2.4Î²2"
#	define VER_RC_REVISION 2, 4, 2, 459
#	define VER_RC_STATUS 2, 4, 2, 1
/**** Subversion Information ****/
#	define VER_REVISION_URL "git@github.com:White-Tiger/T-Clock.git"
#	define VER_REVISION_DATE "2016-09-02 18:02:22 +0000 (Fri, Sep 02 2016)"
#	define VER_REVISION_HASH "6bc075d"
#	define VER_REVISION_TAG "v2.4.2#459-beta"
/**** Date/Time ****/
#	define VER_TIMESTAMP 1472839398
#	define VER_TIME_SEC 18
#	define VER_TIME_MIN 3
#	define VER_TIME_HOUR 18
#	define VER_TIME_DAY 2
#	define VER_TIME_MONTH 9
#	define VER_TIME_YEAR 2016
#	define VER_TIME_WDAY 5
#	define VER_TIME_YDAY 245
#	define VER_TIME_WDAY_SHORT "Fri"
#	define VER_TIME_WDAY_FULL "Friday"
#	define VER_TIME_MONTH_SHORT "Sep"
#	define VER_TIME_MONTH_FULL "September"
#	define VER_TIME "18:03:18"
#	define VER_DATE "2016-09-02"
#	define VER_DATE_LONG "Fri, Sep 02, 2016 18:03:18 UTC"
#	define VER_DATE_SHORT "2016-09-02 18:03:18 UTC"
#	define VER_DATE_ISO "2016-09-02T18:03:18Z"
/**** Helper 'functions' ****/
#	define VER_IsReleaseOrHigher() ( VER_STATUS >= 3 )
#	define VER_IsAlpha() ( VER_STATUS == 0 )
#	define VER_IsBeta() ( VER_STATUS == 1 )
#	define VER_IsRC() ( VER_STATUS == 2 )
#	define VER_IsRelease() ( VER_STATUS == 3 )
#	define VER_IsMaintenance() ( VER_STATUS == 4 )
#ifndef STR
#	define STR_(x) #x
#	define STR(x) STR_(x)
#endif
#ifndef L
#	define L_(x) L##x
#	define L(x) L_(x)
#endif
#endif
