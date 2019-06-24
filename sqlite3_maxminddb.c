/*
**
** This SQLite extension implements asn(), org(), cc() functions.
**
** ipmask(), ip6mask() help you translate to prefixes easily.
*/

#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#ifdef _WIN32
#define HOMEENVNAME "HOMEPATH"
#define PATH_MAX 256
#include "Ws2tcpip.h"
#else
#define HOMEENVNAME "HOME"
#include <linux/limits.h>
#include <arpa/inet.h>
#endif

// maxmind/libmaxminddb
#include "maxminddb.h"

#define MSG_NOTINITIALIZED "sqlite-maxminddb is not initialized"
#ifdef _WIN32
#define MSG_ERRGETADDRINFO "Error from getaddrinfo for %s - %ws"
#else
#define MSG_ERRGETADDRINFO "Error from getaddrinfo for %s - %s"
#endif
#define MSG_ERRLIBMAXMIND  "Got an error from libmaxminddb: %s"

#define KW_ASN     "autonomous_system_number"
#define KW_ORG     "autonomous_system_organization"
#define KW_COUNTRY "country"
#define KW_ISOCODE "iso_code"

bool initialized = false;
MMDB_s mmdb_asn;
MMDB_s mmdb_cnt;

static bool check_lookup(sqlite3_context *context, const char *ipaddress, const int gai_error, const int mmdb_error) {
    if (0 != gai_error) {
        char msg[4096];
        snprintf(msg, 4095, MSG_ERRGETADDRINFO,
                 ipaddress, gai_strerror(gai_error));
        sqlite3_result_error(context, msg, -1);
        return true;
    }

    if (MMDB_SUCCESS != mmdb_error) {
        char msg[4096];
        snprintf(msg, 4095, MSG_ERRLIBMAXMIND,
                 MMDB_strerror(mmdb_error));
        sqlite3_result_error(context, msg, -1);
        return true;
    }

    return false;
}

static void lookup_asn_org(sqlite3_context *context, const char* ipaddress, const char* keyword) {

    assert(keyword != NULL);

    int gai_error, mmdb_error;
    MMDB_lookup_result_s result =
        MMDB_lookup_string(&mmdb_asn, ipaddress, &gai_error, &mmdb_error);
    if(check_lookup(context, ipaddress, gai_error, mmdb_error)){
        return;
    }

    char zOut[4096];
    zOut[0] = '\0';
    if (result.found_entry) {
        MMDB_entry_data_s entry_data;
        int status =
            MMDB_get_value(&result.entry, &entry_data,
                   keyword, NULL);
        if (MMDB_SUCCESS != status) {
            //sqlite3_result_error(context, "MMDB_get_value", -1);
            //return;
        }
        else if (entry_data.has_data) {
            if(entry_data.type==MMDB_DATA_TYPE_UINT32){
                snprintf(zOut, 11, "%u", entry_data.uint32 );
            }
            if(entry_data.type==MMDB_DATA_TYPE_BYTES||
               entry_data.type==MMDB_DATA_TYPE_UTF8_STRING){
                size_t m = 4095;
                if(entry_data.data_size<m) m = entry_data.data_size + 1;
                snprintf(zOut, entry_data.data_size+1, "%s", (const char*) entry_data.bytes );
            }
        }
    }

    sqlite3_result_text(context, (char*) zOut, strlen(zOut), SQLITE_TRANSIENT);
}

static void lookup_country(sqlite3_context *context, const char* ipaddress) {

    int gai_error, mmdb_error;
    MMDB_lookup_result_s result =
        MMDB_lookup_string(&mmdb_cnt, ipaddress, &gai_error, &mmdb_error);
    if(check_lookup(context, ipaddress, gai_error, mmdb_error)){
        return;
    }

    char zOut[4096];
    zOut[0] = '\0';
    if (result.found_entry) {
        MMDB_entry_data_s entry_data;
        int status =
            MMDB_get_value(&result.entry, &entry_data,
                   KW_COUNTRY, KW_ISOCODE, NULL);
        if (MMDB_SUCCESS != status) {
        }
        else if (entry_data.has_data) {
            if(entry_data.type==MMDB_DATA_TYPE_BYTES||
               entry_data.type==MMDB_DATA_TYPE_UTF8_STRING){
                size_t m = 4095;
                if(entry_data.data_size<m) m = entry_data.data_size + 1;
                snprintf(zOut, entry_data.data_size+1,
                    "%s", (const char*) entry_data.bytes );
            }
        }
    }

    sqlite3_result_text(context, (char*) zOut, strlen(zOut), SQLITE_TRANSIENT);
}

static void asn_impl(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv)
{
    const char *zIn;

    assert(argc == 1);

    if(!initialized){
        sqlite3_result_error(context, MSG_NOTINITIALIZED, -1);
        return;
    }

    if (sqlite3_value_type(argv[0]) == SQLITE_NULL)
    {
        return;
    }

    zIn = (const char*)sqlite3_value_text(argv[0]);

    // call sqlite3_result_text or sqlite3_result_error
    lookup_asn_org(context, zIn, KW_ASN);
}

static void org_impl(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv)
{
    const char *zIn;

    assert(argc == 1);

    if(!initialized){
        sqlite3_result_error(context, MSG_NOTINITIALIZED, -1);
        return;
    }

    if (sqlite3_value_type(argv[0]) == SQLITE_NULL)
    {
        return;
    }

    zIn = (const char*) sqlite3_value_text(argv[0]);

    // call sqlite3_result_text or sqlite3_result_error
    lookup_asn_org(context, zIn, KW_ORG);

}

static void cc_impl(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv)
{
    const char *zIn;

    assert(argc == 1);

    if(!initialized){
        sqlite3_result_error(context, MSG_NOTINITIALIZED, -1);
        return;
    }

    if (sqlite3_value_type(argv[0]) == SQLITE_NULL)
    {
        return;
    }

    zIn = (const char*) sqlite3_value_text(argv[0]);

    // call sqlite3_result_text or sqlite3_result_error
    lookup_country(context, zIn);

}

static void ipmask(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const char *ipstr;
    int masklength;
    struct in_addr in4addr;
    char ret[16];

    if (argc != 2) {
        return;
    }
    if (sqlite3_value_type(argv[0]) != SQLITE3_TEXT ||
        sqlite3_value_type(argv[1]) != SQLITE_INTEGER ) {
        return;
    }

    ipstr = (const char*) sqlite3_value_text(argv[0]);
    masklength = sqlite3_value_int(argv[1]);
    if (masklength<0 || masklength>32){
        sqlite3_result_error(context, "Wrong mask length", -1);
        return;
    }

    if (inet_pton(AF_INET, ipstr, &in4addr) != 1) {
        sqlite3_result_error(context, "Passed a malformed IP address", -1);
        return;
    }

    uint32_t mask = 0;
    for(int i=0;i<32-masklength;i++){
        mask |= (1 << i);
    }
    mask = ~mask;
    
    in4addr.s_addr &= ntohl(mask);
    inet_ntop(AF_INET, &in4addr.s_addr, ret, 16);
    sqlite3_result_text(context, (char*) ret, strlen(ret), SQLITE_TRANSIENT);
}

static void ip6mask(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const char *ip6str;
    int masklength;
    struct in6_addr in6addr;
    char ret[40];

    assert(argc == 2);
    if (sqlite3_value_type(argv[0]) != SQLITE3_TEXT) {
        return;
    }
    if (sqlite3_value_type(argv[1]) != SQLITE_INTEGER) {
        return;
    }

    ip6str = (const char*) sqlite3_value_text(argv[0]);
    masklength = sqlite3_value_int(argv[1]);
    if (masklength<0 || masklength>128){
        sqlite3_result_error(context, "Wrong mask length", -1);
        return;
    }

    if (inet_pton(AF_INET6, ip6str, &in6addr) == 0) {
        sqlite3_result_error(context, "Passed a malformed IP address", -1);
        return;
    }

    for(int i=128;i>(masklength-1);i--){
        in6addr.s6_addr[i / 8] &= ~((char) (1 << (7-(i % 8))));
    }
    
    inet_ntop(AF_INET6, in6addr.s6_addr, ret, 40);
    sqlite3_result_text(context, (char*) ret, strlen(ret), SQLITE_TRANSIENT);
}

#ifdef _WIN32
__declspec( dllexport )
#endif
int sqlite3_maxminddb_init( sqlite3 *db,
    char **pzErrMsg, const sqlite3_api_routines *pApi)
{
    int rc = SQLITE_OK;

    SQLITE_EXTENSION_INIT2(pApi);
    (void)pzErrMsg;  /* Unused parameter */

    rc = sqlite3_create_function(
        db,
        "asn",
        1,
        SQLITE_UTF8,
        0,
        asn_impl,
        0,
        0);

    if (rc != SQLITE_OK) {
        return rc;
    }

    rc = sqlite3_create_function(
        db,
        "org",
        1,
        SQLITE_UTF8,
        0,
        org_impl,
        0,
        0);

    if (rc != SQLITE_OK) {
        return rc;
    }

    rc = sqlite3_create_function(
        db,
        "cc",
        1,
        SQLITE_UTF8,
        0,
        cc_impl,
        0,
        0);

    if (rc != SQLITE_OK) {
        return rc;
    }

    rc = sqlite3_create_function(
        db, "ipmask", 2, SQLITE_UTF8, 0,
        ipmask, 0, 0);

    if (rc != SQLITE_OK) {
        return rc;
    }

    rc = sqlite3_create_function(
        db, "ip6mask", 2, SQLITE_UTF8, 0,
        ip6mask, 0, 0);

    if (rc != SQLITE_OK) {
        return rc;
    }

    char* HOME = getenv(HOMEENVNAME);
    char GEOLITE2_ASN[PATH_MAX];
    char GEOLITE2_CNT[PATH_MAX];
    sprintf(GEOLITE2_ASN, "%s/.maxminddb/GeoLite2-ASN.mmdb", HOME);
    sprintf(GEOLITE2_CNT, "%s/.maxminddb/GeoLite2-Country.mmdb", HOME);
    int status_asn = MMDB_open(GEOLITE2_ASN, MMDB_MODE_MMAP, &mmdb_asn);
    int status_cnt = MMDB_open(GEOLITE2_CNT, MMDB_MODE_MMAP, &mmdb_cnt);
    if (MMDB_SUCCESS != status_asn || MMDB_SUCCESS != status_cnt) {
    } else {
        initialized = true;
        rc = SQLITE_OK;
    }

    return rc;
}
