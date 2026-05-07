#ifndef CONFIG_TYPE_H
#define CONFIG_TYPE_H

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Configuration type definitions
#define LAB 4
#define NH_OPEN_CABIN 5
#define NH_CLOSE_CABIN 6
#define TEST 8

#ifndef CONFIG_TYPE
#define CONFIG_TYPE TEST
#endif

// Configuration name strings
#if CONFIG_TYPE == LAB
    #define CONFIG_TYPE_STR "LAB"
#elif CONFIG_TYPE == NH_OPEN_CABIN
    #define CONFIG_TYPE_STR "NH_OPEN_CABIN"
#elif CONFIG_TYPE == NH_CLOSE_CABIN
    #define CONFIG_TYPE_STR "NH_CLOSE_CABIN"
#elif CONFIG_TYPE == TEST
    #define CONFIG_TYPE_STR "TEST"
#else
    #define CONFIG_TYPE_STR "UNKNOWN_CONFIG"
#endif

// Configuration-specific includes
#if CONFIG_TYPE == LAB
    #include "Lab/f072_lab_config_memory.hpp"
#elif CONFIG_TYPE == NH_OPEN_CABIN
    #include "NHOpenCabin/f072_nh_open_cabin_config_memory.hpp"
#elif CONFIG_TYPE == NH_CLOSE_CABIN
    #include "NHCloseCabin/f072_nh_close_cabin_config_memory.hpp"
#elif CONFIG_TYPE == TEST
    #include "Test/f072_test_config_memory.hpp"
#endif

#endif // CONFIG_TYPE_H
