#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "maxminddb.h"
#include <sqlite3ext.h>

#ifdef _WIN32
#   define MSG_ERRGETADDRINFO "Error from getaddrinfo for %s - %ws"
#   define HOMEENVNAME "HOMEPATH"
#   define DLLFUNC __declspec(dllexport)
#   define PATH_MAX 260
#   include <Ws2tcpip.h>
#   include <direct.h>
#else
#   define MSG_ERRGETADDRINFO "Error from getaddrinfo for %s - %s"
#   define HOMEENVNAME "HOME"
#   define DLLFUNC
#   include <linux/limits.h>
#   include <arpa/inet.h>
#   include <unistd.h>
#endif

enum {
    GEOIP_FUNCTION_COUNTRY,          /**< enum value for determining if the selected function is "geoip_country" */
    GEOIP_FUNCTION_CONTINENT,        /**< enum value for determining if the selected function is "geoip_continent" */
    GEOIP_FUNCTION_CITY,             /**< enum value for determining if the selected function is "geoip_city" */
    GEOIP_FUNCTION_STATE,            /**< enum value for determining if the selected function is "geoip_state" */
    GEOIP_FUNCTION_TIMEZONE,         /**< enum value for determining if the selected function is "geoip_timezone" */
    GEOIP_FUNCTION_ZIPCODE,          /**< enum value for determining if the selected function is "geoip_zipcode" */
    GEOIP_FUNCTION_ASN_ORGANIZATION, /**< enum value for determining if the selected function is "geoip_asn_owner" */
    GEOIP_FUNCTION_ASN_NUMBER        /**< enum value for determining if the selected function is "geoip_asn_number" */
};

#define MSG_NOTINITIALIZED "sqlite-maxminddb is not initialized"
#define MSG_ERRLIBMAXMIND  "Got an error from libmaxminddb: %s"
SQLITE_EXTENSION_INIT1

bool initialized = false; /**< A global variable that is being used to determine if the SQLite3 extension is ready to be used or not. */
MMDB_s mmdb_asn;          /**< A global variable that will be used to store and reference the contents of the GeoLite2-ASN MMDB file. */
MMDB_s mmdb_cnt;          /**< A global variable that will be used to store and reference the contents of the GeoLite2-City MMDB file. */

/**
 * Convert an enum value to a string value.
 * 
 * Retrieve a human-readable equivalent value for a preprocessor defined enumerator value.
 * 
 * @param type A numerical value that is cross-referenced with a header file to determine what data type is being referenced.
 */
static const char *MMDB_get_typestr(const uint32_t type) {
    switch (type) {
    case MMDB_DATA_TYPE_EXTENDED:     /**< If the data type is an extended data type */
        return "extended";
    case MMDB_DATA_TYPE_POINTER:      /**< If the data type is a pointer to a memory location */
        return "pointer";
    case MMDB_DATA_TYPE_UTF8_STRING:  /**< If the data type is a pointer to a memory location storing signed 7-bit ASCII characters */
        return "UTF8 string";
    case MMDB_DATA_TYPE_DOUBLE:       /**< If the data type is a double precision floating point value */
        return "double";
    case MMDB_DATA_TYPE_BYTES:        /**< If the data type is a pointer to a memory location storing unsigned 8-bit characters */
        return "bytes";
    case MMDB_DATA_TYPE_UINT16:       /**< If the data type is an unsigned 16-bit integer */
        return "short int";
    case MMDB_DATA_TYPE_UINT32:       /**< If the data type is an unsigned 32-bit integer */
        return "unsigned int";
    case MMDB_DATA_TYPE_MAP:          /**< If the data type is a map */
        return "map";
    case MMDB_DATA_TYPE_INT32:        /**< If the data type is a 32-bit integer */
        return "signed int";
    case MMDB_DATA_TYPE_UINT64:       /**< If the data type is an unsigned 64-bit integer */
        return "long int";
    case MMDB_DATA_TYPE_UINT128:      /**< If the data type is an unsigned 128-bit integer */
        return "128-bit integer";
    case MMDB_DATA_TYPE_ARRAY:        /**< If the data type is an array */
        return "array";
    case MMDB_DATA_TYPE_CONTAINER:    /**< If the data type is a container */
        return "container";
    case MMDB_DATA_TYPE_END_MARKER:   /**< If the data type is an end-marker */
        return "end marker";
    case MMDB_DATA_TYPE_BOOLEAN:      /**< If the data type is a true or false value */
        return "boolean";
    case MMDB_DATA_TYPE_FLOAT:        /**< If the data type is a floating point value */
        return "float";
    default:                          /**< Fallthrough to access an otherwise inaccessible return statement. */
    };

    return "unknown";
}

/**
 * Determine if errors occurred during runtime.
 * 
 * This function will be used to check if 'gai_error' and 'mmdb_error' are set to values that determine whether or not an error occurred.
 * 
 * @param context       The current SQLite3 function context structure/object.
 * @param ipaddress     A pointer to a location in memory that stores a string of characters that represent the IP address.
 * @param gai_error     A numerical value that should equal 0 to suggest no issues, otherwise there's DNS resolution issues.
 * @param mmdb_error    A numerical value that references an enum that stores a list of potential issues with MaxMindDB.
 */
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

/**
 * Retrieve data from all extension functions.
 * 
 * This function will access all available fields in the MMDB file and concatenate their results into a single string of characters.
 * 
 * @param context       The current SQLite3 function context structure/object.
 * @param status        The MMDB error status which should be 0.
 * @param zOut          The string containing the IP address passed by the "geoip" function.
 * @param entry_data    An MMDB structure that stores field information for the queried data.
 * @return              The concatenated string that stores all field data from both MMDB databases.
 */
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

/**
 * Send the results of the MMDB query to SQLite
 * 
 * This function handles the bulk of this extension's functionality by reflecting the MMDB query results to the SQLite query.
 * 
 * @param context       The current SQLite3 function context structure/object.
 * @param status        The MMDB error status which should be 0.
 * @param zOut          The string containing the IP address passed by the "geoip" function.
 * @param entry_data    An MMDB structure that stores field information for the queried data.   
 */
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

/**
 * Perform an MMDB query.
 * 
 * This function performs the MMDB queries and sets up the extension to return the results of the query to SQLite.
 * 
 * @param context       The current SQLite3 function context structure/object.
 * @param ipaddress     A pointer to a location in memory that stores a string of characters that represent the IP address.
 * @param mmdb          A reference to the correct MMDB file that stores the table members for ASNs versus Cities.
 * @param functype      An enum that represents which of the extension functions is being called to build the correct MMDB query.
 * 
 * @todo Rename this function since it used to have variadic arguments instead of the enum "functype".
 */
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

/**
 * Perform queries on both MMDB databases.
 * 
 * This function will simply run queries on both MMDB databases to be passed along to the "get_data" function.
 * 
 * @param context       The current SQLite3 function context structure/object.
 * @param ipaddress     A pointer to a location in memory that stores a string of characters that represent the IP address.
 */
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

/**
 * Perform a query on the GeoLite2-City MMDB database.
 * 
 * This function handles the "geoip_country" extension function.
 * 
 * @param context       The current SQLite3 function context structure/object.
 * @param argc          The number of arguments passed to the SQLite function (should be 1).
 * @param argv          The contents of the arguments passed to the SQLite function.
 */
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

/**
 * Perform a query on the GeoLite2-City MMDB database.
 * 
 * This function handles the "geoip_continent" extension function.
 * 
 * @param context       The current SQLite3 function context structure/object.
 * @param argc          The number of arguments passed to the SQLite function (should be 1).
 * @param argv          The contents of the arguments passed to the SQLite function.
 */
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

/**
 * Perform a query on the GeoLite2-City MMDB database.
 * 
 * This function handles the "geoip_city" extension function.
 * 
 * @param context       The current SQLite3 function context structure/object.
 * @param argc          The number of arguments passed to the SQLite function (should be 1).
 * @param argv          The contents of the arguments passed to the SQLite function.
 */
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

/**
 * Perform a query on the GeoLite2-City MMDB database.
 * 
 * This function handles the "geoip_state" extension function.
 * 
 * @param context       The current SQLite3 function context structure/object.
 * @param argc          The number of arguments passed to the SQLite function (should be 1).
 * @param argv          The contents of the arguments passed to the SQLite function.
 */
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

/**
 * Perform a query on the GeoLite2-City MMDB database.
 * 
 * This function handles the "geoip_timezone" extension function.
 * 
 * @param context       The current SQLite3 function context structure/object.
 * @param argc          The number of arguments passed to the SQLite function (should be 1).
 * @param argv          The contents of the arguments passed to the SQLite function.
 */
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

/**
 * Perform a query on the GeoLite2-City MMDB database.
 * 
 * This function handles the "geoip_zipcode" extension function.
 * 
 * @param context       The current SQLite3 function context structure/object.
 * @param argc          The number of arguments passed to the SQLite function (should be 1).
 * @param argv          The contents of the arguments passed to the SQLite function.
 */
static void lookup_zip(sqlite3_context *context, int argc, sqlite3_value **argv) {
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

/**
 * Perform a query on the GeoLite2-ASN MMDB database.
 * 
 * This function handles the "geoip_asn_owned" extension function.
 * 
 * @param context       The current SQLite3 function context structure/object.
 * @param argc          The number of arguments passed to the SQLite function (should be 1).
 * @param argv          The contents of the arguments passed to the SQLite function.
 */
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

/**
 * Perform a query on the GeoLite2-ASN MMDB database.
 * 
 * This function handles the "geoip_asn_number" extension function.
 * 
 * @param context       The current SQLite3 function context structure/object.
 * @param argc          The number of arguments passed to the SQLite function (should be 1).
 * @param argv          The contents of the arguments passed to the SQLite function.
 */
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

/**
 * Perform a query on both MMDB databases.
 * 
 * This function handles the "geoip" extension function.
 * 
 * @param context       The current SQLite3 function context structure/object.
 * @param argc          The number of arguments passed to the SQLite function (should be 1).
 * @param argv          The contents of the arguments passed to the SQLite function.
 */
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

/**
 * The SQLite3 hooking function.
 * 
 * This function serves as the entrypoint for all SQLite3 extensions.
 * 
 * @param db        The current SQLite3 database context.
 * @param pzErrMsg  A useless function in this context, normally used to store extension startup error messages.
 * @param pApi      Used to set up the necessary SQLite3 API functions within the scope of this extension.
 * @return          An error code that SQLite will use to determine what went wrong (should be 0).
 */
DLLFUNC int sqlite3_maxminddbext_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi) {
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

    char HOME[PATH_MAX];

    #ifdef _WIN32
        if (_getcwd(HOME, sizeof(HOME)) == NULL) {
            perror("_getcwd() error");
            return rc;
        }
    #else
        if (getcwd(HOME, PATH_MAX) == NULL) {
            perror("getcwd() error");
            return rc;
        }
    #endif

    char GEOLITE2_ASN[PATH_MAX];
    char GEOLITE2_CTY[PATH_MAX];

    sprintf(GEOLITE2_ASN, "%s/GeoLite2-ASN.mmdb", HOME);
    sprintf(GEOLITE2_CTY, "%s/GeoLite2-City.mmdb", HOME);

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
