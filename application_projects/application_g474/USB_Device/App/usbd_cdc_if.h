/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbd_cdc_if.h
  * @brief          : Header for usbd_cdc_if.c file.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __USBD_CDC_IF_H__
#define __USBD_CDC_IF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "usbd_cdc.h"

#define APP_RX_DATA_SIZE 2048U
#define APP_TX_DATA_SIZE 2048U

extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;
extern USBD_HandleTypeDef hUsbDeviceFS;

extern bool usb_transmit_flag;
extern uint16_t usb_receive_size;
extern uint8_t host_connection;

uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CDC_IF_H__ */
