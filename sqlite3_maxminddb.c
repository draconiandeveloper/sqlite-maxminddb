/*
**
** This SQLite extension originally implemented asn(), org(), cc(), ipmask(), and ip6mask() functions.
**
** Now this function has been modified to include as many options from the GeoLite2 City MMDB and GeoLite2 ASN MMDB dbs.
**
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "maxminddb.h"
#include <sqlite3ext.h>

#ifdef _WIN32
#   define MSG_ERRGETADDRINFO "Error from getaddrinfo for %s - %ws"
#   define HOMEENVNAME "HOMEPATH"
#   define PATH_MAX 260
#   include "Ws2tcpip.h"
#else
#   define MSG_ERRGETADDRINFO "Error from getaddrinfo for %s - %s"
#   define HOMEENVNAME "HOME"
#   include <linux/limits.h>
#   include <arpa/inet.h>
#endif

enum {
    GEOIP_FUNCTION_COUNTRY,
    GEOIP_FUNCTION_CONTINENT,
    GEOIP_FUNCTION_CITY,
    GEOIP_FUNCTION_STATE,
    GEOIP_FUNCTION_TIMEZONE,
    GEOIP_FUNCTION_ZIPCODE,
    GEOIP_FUNCTION_ASN_ORGANIZATION,
    GEOIP_FUNCTION_ASN_NUMBER
};

#define MSG_NOTINITIALIZED "sqlite-maxminddb is not initialized"
#define MSG_ERRLIBMAXMIND  "Got an error from libmaxminddb: %s"
SQLITE_EXTENSION_INIT1

bool initialized = false;
MMDB_s mmdb_asn;
MMDB_s mmdb_cnt;

static const char *MMDB_get_typestr(const uint32_t type) {
    switch (type) {
    case MMDB_DATA_TYPE_EXTENDED:
        return "extended";
    case MMDB_DATA_TYPE_POINTER:
        return "pointer";
    case MMDB_DATA_TYPE_UTF8_STRING:
        return "UTF8 string";
    case MMDB_DATA_TYPE_DOUBLE:
        return "double";
    case MMDB_DATA_TYPE_BYTES:
        return "bytes";
    case MMDB_DATA_TYPE_UINT16:
        return "short int";
    case MMDB_DATA_TYPE_UINT32:
        return "unsigned int";
    case MMDB_DATA_TYPE_MAP:
        return "map";
    case MMDB_DATA_TYPE_INT32:
        return "signed int";
    case MMDB_DATA_TYPE_UINT64:
        return "long int";
    case MMDB_DATA_TYPE_UINT128:
        return "128-bit integer";
    case MMDB_DATA_TYPE_ARRAY:
        return "array";
    case MMDB_DATA_TYPE_CONTAINER:
        return "container";
    case MMDB_DATA_TYPE_END_MARKER:
        return "end marker";
    case MMDB_DATA_TYPE_BOOLEAN:
        return "boolean";
    case MMDB_DATA_TYPE_FLOAT:
        return "float";
    default: // Fallthrough
    };

    return "unknown";
}

static bool check_lookup(sqlite3_context *context, const char *ipaddress, const int gai_error, const int mmdb_error) {
    if (gai_error != 0) {
        char msg[4096];

        snprintf(msg, 4095, MSG_ERRGETADDRINFO, ipaddress, gai_strerror(gai_error));
        sqlite3_result_error(context, msg, -1);
        return true;
    }

    if (MMDB_SUCCESS != mmdb_error) {
        char msg[4096];

        snprintf(msg, 4095, MSG_ERRLIBMAXMIND, MMDB_strerror(mmdb_error));
        sqlite3_result_error(context, msg, -1);
        return true;
    }

    return false;
}

static char *get_data(sqlite3_context *context, int status, char *zOut, MMDB_entry_data_s entry_data) {
    assert(zOut != NULL);

    if (status != MMDB_SUCCESS)
        return "NULL";

    if (!entry_data.has_data)
        return "NULL";
    
    if (entry_data.type == MMDB_DATA_TYPE_BYTES || entry_data.type == MMDB_DATA_TYPE_UTF8_STRING)
        snprintf(zOut, entry_data.data_size + 1, "%s", (const char *)entry_data.bytes);
    else if (entry_data.type == MMDB_DATA_TYPE_UINT32)
        snprintf(zOut, entry_data.data_size + 1, "%u", entry_data.uint32);
    else
        return "NULL";

    return zOut;
} 

static void send_data(sqlite3_context *context, int status, char *zOut, MMDB_entry_data_s entry_data) {
    assert(zOut != NULL);
    char errmsg[PATH_MAX];

    if (status != MMDB_SUCCESS) {
        sprintf(errmsg, " %d: %s", status, MMDB_strerror(status));
        sqlite3_result_error(context, errmsg, -1);
        return;
    }

    if (!entry_data.has_data) {
        sprintf(errmsg, "No data to retrieve");
        sqlite3_result_error(context, errmsg, -1);
        return;
    }

    if (entry_data.type == MMDB_DATA_TYPE_BYTES || entry_data.type == MMDB_DATA_TYPE_UTF8_STRING)
        snprintf(zOut, entry_data.data_size + 1, "%s", (const char *)entry_data.bytes);
    else if (entry_data.type == MMDB_DATA_TYPE_UINT32)
        snprintf(zOut, entry_data.data_size + 1, "%u", entry_data.uint32);
    else {
        sprintf(errmsg, "Data type is: %s", MMDB_get_typestr(entry_data.type));
        sqlite3_result_error(context, errmsg, -1);
    }

    sqlite3_result_text(context, (char *)zOut, strlen(zOut), SQLITE_TRANSIENT);
}

static void lookup_vargs(sqlite3_context *context, const char *ipaddress, const MMDB_s *mmdb, int functype) {
    assert(functype >= 0 && functype <= 7);

    int gai_error, mmdb_error;
    MMDB_lookup_result_s result = MMDB_lookup_string(mmdb, ipaddress, &gai_error, &mmdb_error);
    if (check_lookup(context, ipaddress, gai_error, mmdb_error)) {
        char errmsg[PATH_MAX];

        if (gai_error) {
            sprintf(errmsg, " (%d): %s", gai_error, gai_strerror(gai_error));
            sqlite3_result_error(context, errmsg, -1);
            return;
        }

        sprintf(errmsg, " (%d): %s", mmdb_error, MMDB_strerror(mmdb_error));
        sqlite3_result_error(context, errmsg, -1);
        return;
    }

    char zOut[4096];
    zOut[0] = '\0';

    if (result.found_entry) {
        MMDB_entry_data_s entry_data;
        int status = 0;

        switch (functype) {
        case GEOIP_FUNCTION_COUNTRY:
            status = MMDB_get_value(&result.entry, &entry_data, "country", "names", "en", NULL);
            send_data(context, status, zOut, entry_data);
            break;
        case GEOIP_FUNCTION_CONTINENT:
            status = MMDB_get_value(&result.entry, &entry_data, "continent", "names", "en", NULL);
            send_data(context, status, zOut, entry_data);
            break;
        case GEOIP_FUNCTION_CITY:
            status = MMDB_get_value(&result.entry, &entry_data, "city", "names", "en", NULL);
            send_data(context, status, zOut, entry_data);
            break;
        case GEOIP_FUNCTION_STATE:
            status = MMDB_get_value(&result.entry, &entry_data, "subdivisions", "0", "names", "en", NULL);
            send_data(context, status, zOut, entry_data);
            break;
        case GEOIP_FUNCTION_TIMEZONE:
            status = MMDB_get_value(&result.entry, &entry_data, "location", "time_zone", NULL);
            send_data(context, status, zOut, entry_data);
            break;
        case GEOIP_FUNCTION_ZIPCODE:
            status = MMDB_get_value(&result.entry, &entry_data, "postal", "code", NULL);
            send_data(context, status, zOut, entry_data);
            break;
        case GEOIP_FUNCTION_ASN_ORGANIZATION:
            status = MMDB_get_value(&result.entry, &entry_data, "autonomous_system_organization", NULL);
            send_data(context, status, zOut, entry_data);
            break;
        case GEOIP_FUNCTION_ASN_NUMBER:
            status = MMDB_get_value(&result.entry, &entry_data, "autonomous_system_number", NULL);
            send_data(context, status, zOut, entry_data);
            break;
        default:
            break;
        };
    }
}

static void lookup_all(sqlite3_context *context, const char *ipaddress) {
    int gai_error, mmdb_error;

    MMDB_lookup_result_s result_asn = MMDB_lookup_string(&mmdb_asn, ipaddress, &gai_error, &mmdb_error);
    if (check_lookup(context, ipaddress, gai_error, mmdb_error)) {
        char errmsg[PATH_MAX];

        if (gai_error) {
            sprintf(errmsg, " (%d): %s", gai_error, gai_strerror(gai_error));
            sqlite3_result_error(context, errmsg, -1);
            return;
        }

        sprintf(errmsg, " (%d): %s", mmdb_error, MMDB_strerror(mmdb_error));
        sqlite3_result_error(context, errmsg, -1);
        return;
    }

    MMDB_lookup_result_s result_cnt = MMDB_lookup_string(&mmdb_cnt, ipaddress, &gai_error, &mmdb_error);
    if (check_lookup(context, ipaddress, gai_error, mmdb_error)) {
        char errmsg[PATH_MAX];

        if (gai_error) {
            sprintf(errmsg, " (%d): %s", gai_error, gai_strerror(gai_error));
            sqlite3_result_error(context, errmsg, -1);
            return;
        }

        sprintf(errmsg, " (%d): %s", mmdb_error, MMDB_strerror(mmdb_error));
        sqlite3_result_error(context, errmsg, -1);
        return;
    }

    char zOut[4096];
    zOut[0] = '\0';

    char zData[4096];
    zData[0] = '\0';

    MMDB_entry_data_s entry_data;

    if (result_asn.found_entry) {
        MMDB_get_value(&result_asn.entry, &entry_data, "autonomous_system_organization", NULL);
        strcat(zData, get_data(context, 0, zOut, entry_data));
        strcat(zData, " | ");

        MMDB_get_value(&result_asn.entry, &entry_data, "autonomous_system_number", NULL);
        strcat(zData, get_data(context, 0, zOut, entry_data));
        strcat(zData, " | ");
    } else {
        strcat(zData, "NULL | NULL | ");
    }

    if (result_cnt.found_entry) {
        MMDB_get_value(&result_cnt.entry, &entry_data, "continent", "names", "en", NULL);
        strcat(zData, get_data(context, 0, zOut, entry_data));
        strcat(zData, " | ");

        MMDB_get_value(&result_cnt.entry, &entry_data, "country", "names", "en", NULL);
        strcat(zData, get_data(context, 0, zOut, entry_data));
        strcat(zData, " | ");
        
        MMDB_get_value(&result_cnt.entry, &entry_data, "subdivisions", "0", "names", "en", NULL);
        strcat(zData, get_data(context, 0, zOut, entry_data));
        strcat(zData, " | ");
        
        MMDB_get_value(&result_cnt.entry, &entry_data, "city", "names", "en", NULL);
        strcat(zData, get_data(context, 0, zOut, entry_data));
        strcat(zData, " | ");

        MMDB_get_value(&result_cnt.entry, &entry_data, "postal", "code", NULL);
        strcat(zData, get_data(context, 0, zOut, entry_data));
        strcat(zData, " | ");

        MMDB_get_value(&result_cnt.entry, &entry_data, "location", "time_zone", NULL);
        strcat(zData, get_data(context, 0, zOut, entry_data));

        sqlite3_result_text(context, (char *)zData, strlen(zData), SQLITE_TRANSIENT);
    }
}

static void lookup_country(sqlite3_context *context, int argc, sqlite3_value **argv) { 
    assert(argc == 1);
    const char *zIn;

    if (!initialized) {
        sqlite3_result_error(context, MSG_NOTINITIALIZED, -1);
        return;
    }

    if (sqlite3_value_type(argv[0]) == SQLITE_NULL)
        return;
    
    zIn = (const char *)sqlite3_value_text(argv[0]);
    lookup_vargs(context, zIn, &mmdb_cnt, GEOIP_FUNCTION_COUNTRY);          
}

static void lookup_continent(sqlite3_context *context, int argc, sqlite3_value **argv) {
    assert(argc == 1);
    const char *zIn;

    if (!initialized) {
        sqlite3_result_error(context, MSG_NOTINITIALIZED, -1);
        return;
    }

    if (sqlite3_value_type(argv[0]) == SQLITE_NULL)
        return;
    
    zIn = (const char *)sqlite3_value_text(argv[0]);
    lookup_vargs(context, zIn, &mmdb_cnt, GEOIP_FUNCTION_CONTINENT);
}

static void lookup_city(sqlite3_context *context, int argc, sqlite3_value **argv) { 
    assert(argc == 1);
    const char *zIn;

    if (!initialized) {
        sqlite3_result_error(context, MSG_NOTINITIALIZED, -1);
        return;
    }

    if (sqlite3_value_type(argv[0]) == SQLITE_NULL)
        return;
    
    zIn = (const char *)sqlite3_value_text(argv[0]);
    lookup_vargs(context, zIn, &mmdb_cnt, GEOIP_FUNCTION_CITY);
}

static void lookup_state(sqlite3_context *context, int argc, sqlite3_value **argv) { 
    assert(argc == 1);
    const char *zIn;

    if (!initialized) {
        sqlite3_result_error(context, MSG_NOTINITIALIZED, -1);
        return;
    }

    if (sqlite3_value_type(argv[0]) == SQLITE_NULL)
        return;
    
    zIn = (const char *)sqlite3_value_text(argv[0]);
    lookup_vargs(context, zIn, &mmdb_cnt, GEOIP_FUNCTION_STATE);
}

static void lookup_tz(sqlite3_context *context, int argc, sqlite3_value **argv) { 
    assert(argc == 1);
    const char *zIn;

    if (!initialized) {
        sqlite3_result_error(context, MSG_NOTINITIALIZED, -1);
        return;
    }

    if (sqlite3_value_type(argv[0]) == SQLITE_NULL)
        return;
    
    zIn = (const char *)sqlite3_value_text(argv[0]);
    lookup_vargs(context, zIn, &mmdb_cnt, GEOIP_FUNCTION_TIMEZONE);
}

static void lookup_zip(sqlite3_context *context, int argc, sqlite3_value **argv) { \
    assert(argc == 1);
    const char *zIn;

    if (!initialized) {
        sqlite3_result_error(context, MSG_NOTINITIALIZED, -1);
        return;
    }

    if (sqlite3_value_type(argv[0]) == SQLITE_NULL)
        return;
    
    zIn = (const char *)sqlite3_value_text(argv[0]);
    lookup_vargs(context, zIn, &mmdb_cnt, GEOIP_FUNCTION_ZIPCODE);
}

static void lookup_org(sqlite3_context *context, int argc, sqlite3_value **argv) { 
    assert(argc == 1);
    const char *zIn;

    if (!initialized) {
        sqlite3_result_error(context, MSG_NOTINITIALIZED, -1);
        return;
    }

    if (sqlite3_value_type(argv[0]) == SQLITE_NULL)
        return;
    
    zIn = (const char *)sqlite3_value_text(argv[0]);
    lookup_vargs(context, zIn, &mmdb_asn, GEOIP_FUNCTION_ASN_ORGANIZATION); 
}

static void lookup_asn(sqlite3_context *context, int argc, sqlite3_value **argv) { 
    assert(argc == 1);
    const char *zIn;

    if (!initialized) {
        sqlite3_result_error(context, MSG_NOTINITIALIZED, -1);
        return;
    }

    if (sqlite3_value_type(argv[0]) == SQLITE_NULL)
        return;
    
    zIn = (const char *)sqlite3_value_text(argv[0]);
    lookup_vargs(context, zIn, &mmdb_asn, GEOIP_FUNCTION_ASN_NUMBER);
}

static void lookup_geoip(sqlite3_context *context, int argc, sqlite3_value **argv) {
    assert(argc == 1);
    const char *zIn;

    if (!initialized) {
        sqlite3_result_error(context, MSG_NOTINITIALIZED, -1);
        return;
    }

    if (sqlite3_value_type(argv[0]) == SQLITE_NULL)
        return;
    
    zIn = (const char *)sqlite3_value_text(argv[0]);
    lookup_all(context, zIn);
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_maxminddb_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi) {
    int rc = SQLITE_OK;

    SQLITE_EXTENSION_INIT2(pApi);
    (void)pzErrMsg;  /* Unused parameter */

    rc = sqlite3_create_function(db, "geoip_asn_number", 1, SQLITE_UTF8, 0, lookup_asn, 0, 0);
    if (rc != SQLITE_OK) return rc;

    rc = sqlite3_create_function(db, "geoip_asn_owner", 1, SQLITE_UTF8, 0, lookup_org, 0, 0);
    if (rc != SQLITE_OK) return rc;

    rc = sqlite3_create_function(db, "geoip_timezone", 1, SQLITE_UTF8, 0, lookup_tz, 0, 0);
    if (rc != SQLITE_OK) return rc;

    rc = sqlite3_create_function(db, "geoip_zipcode", 1, SQLITE_UTF8, 0, lookup_zip, 0, 0);
    if (rc != SQLITE_OK) return rc;

    rc = sqlite3_create_function(db, "geoip_continent", 1, SQLITE_UTF8, 0, lookup_continent, 0, 0);
    if (rc != SQLITE_OK) return rc;

    rc = sqlite3_create_function(db, "geoip_country", 1, SQLITE_UTF8, 0, lookup_country, 0, 0);
    if (rc != SQLITE_OK) return rc;

    rc = sqlite3_create_function(db, "geoip_state", 1, SQLITE_UTF8, 0, lookup_state, 0, 0);
    if (rc != SQLITE_OK) return rc;

    rc = sqlite3_create_function(db, "geoip_city", 1, SQLITE_UTF8, 0, lookup_city, 0, 0);
    if (rc != SQLITE_OK) return rc;

    rc = sqlite3_create_function(db, "geoip", 1, SQLITE_UTF8, 0, lookup_geoip, 0, 0);
    if (rc != SQLITE_OK) return rc;

    const char *HOME = getenv(HOMEENVNAME);
    char GEOLITE2_ASN[PATH_MAX];
    char GEOLITE2_CTY[PATH_MAX];

    sprintf(GEOLITE2_ASN, "%s/.maxminddb/GeoLite2-ASN.mmdb", HOME);
    sprintf(GEOLITE2_CTY, "%s/.maxminddb/GeoLite2-City.mmdb", HOME);

    int status_asn = MMDB_open(GEOLITE2_ASN, MMDB_MODE_MMAP, &mmdb_asn);
    int status_cnt = MMDB_open(GEOLITE2_CTY, MMDB_MODE_MMAP, &mmdb_cnt);

    if (status_asn != MMDB_SUCCESS)
        fprintf(stderr, "Error (%d): %s on GeoLitev2 ASN MMDB\n", status_asn, MMDB_strerror(status_asn));
    
    if (status_cnt != MMDB_SUCCESS)
        fprintf(stderr, "Error (%d): %s on GeoLitev2 City MMDB\n", status_cnt, MMDB_strerror(status_cnt));

    if (status_asn != MMDB_SUCCESS || status_cnt != MMDB_SUCCESS) {} else {
        initialized = true;
        rc = SQLITE_OK;
    }

    return rc;
}
