#ifndef VERSIONS_H
#define VERSIONS_H

#define APPLICATION_VERSION_MAJOR 0U
#define APPLICATION_VERSION_MINOR 0U
#define APPLICATION_VERSION_PATCH 0U

#define BOOTLOADER_VERSION_MAJOR 0U
#define BOOTLOADER_VERSION_MINOR 0U
#define BOOTLOADER_VERSION_PATCH 0U

#define CONFIG_READ_ONLY_MEMORY_VERSION_MAJOR 0U
#define CONFIG_READ_ONLY_MEMORY_VERSION_MINOR 2U
#define CONFIG_READ_ONLY_MEMORY_VERSION_PATCH 0U

#define CONFIG_MODIFIABLE_MEMORY_VERSION_MAJOR 0U
#define CONFIG_MODIFIABLE_MEMORY_VERSION_MINOR 0U
#define CONFIG_MODIFIABLE_MEMORY_VERSION_PATCH 0U

// Git commit information - will be populated at build time
#ifndef GIT_COMMIT
#define GIT_COMMIT "unknown"
#endif

/*
 * Adding new variables at the end of the MODIFIABLE_MEMORY struct does not require a version upgrade."
 * To avoid memory rewrite, it is recommended not to update the MODIFIABLE_MEMORY_VERSION unless you modify the order of
 * the MODIFIABLE_MEMORY struct.
 */

#endif  // VERSIONS_H
