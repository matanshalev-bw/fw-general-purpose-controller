#ifndef CONFIG_TYPE_H
#define CONFIG_TYPE_H

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Configuration type definitions
#define LAB 1
#define NH_OPEN_CABIN 2
#define NH_CLOSE_CABIN 3
#define TEST 7

#ifndef CONFIG_TYPE
#define CONFIG_TYPE LAB
#endif

// Configuration name strings
#if CONFIG_TYPE == LAB
    #define CONFIG_TYPE_STR "G474_LAB"
#elif CONFIG_TYPE == NH_OPEN_CABIN
    #define CONFIG_TYPE_STR "G474_NH_OPEN_CABIN"
#elif CONFIG_TYPE == NH_CLOSE_CABIN
    #define CONFIG_TYPE_STR "G474_NH_CLOSE_CABIN"
#elif CONFIG_TYPE == TEST
    #define CONFIG_TYPE_STR "G474_TEST"
#else
    #define CONFIG_TYPE_STR "UNKNOWN_CONFIG"
#endif

// Configuration-specific includes
#if CONFIG_TYPE == LAB
    #include "Lab/g474_lab_config_memory.hpp"
#elif CONFIG_TYPE == NH_OPEN_CABIN
    #include "NHOpenCabin/g474_nh_open_cabin_config_memory.hpp"
#elif CONFIG_TYPE == NH_CLOSE_CABIN
    #include "NHCloseCabin/g474_nh_close_cabin_config_memory.hpp"
#elif CONFIG_TYPE == TEST
    #include "Test/g474_test_config_memory.hpp"
#endif

#endif // CONFIG_TYPE_H
