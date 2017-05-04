/*==========================================================================
dbcmd
config.h
Copyright (c)2017 Kevin Boone, GPLv3.0
*==========================================================================*/

#pragma once

// These arguments will be added to every curl invocation. You probably need
// "--cacert /path/to/cacert.pem" on systems that have a curl that is built
// without default CA certificates. Or just "-k" to turn off server cert
// validation completely, if you trust your ISP to route requests for
// Dropbox correctly ;)
#define CURL_EXTRA "-k"
