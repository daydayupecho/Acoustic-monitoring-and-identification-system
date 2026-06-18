#ifndef APP_AZURE_RTOS_CONFIG_H
#define APP_AZURE_RTOS_CONFIG_H
#ifdef __cplusplus
extern "C" {
#endif

#define USE_STATIC_ALLOCATION                    1
/* Increased for A/B/C stacks plus deeper feature/free-slot queues. */
#define TX_APP_MEM_POOL_SIZE                     (24 * 1024)

#ifdef __cplusplus
}
#endif
#endif /* APP_AZURE_RTOS_CONFIG_H */
