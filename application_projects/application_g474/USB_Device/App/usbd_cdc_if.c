/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbd_cdc_if.c
  * @brief          : USB CDC interface for application messaging (no UART bridge).
  ******************************************************************************
  */
/* USER CODE END Header */

#include "usbd_cdc_if.h"

/* USER CODE BEGIN INCLUDE */

/* USER CODE END INCLUDE */

uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

/* USER CODE BEGIN PRIVATE_VARIABLES */
bool usb_transmit_flag = true;
uint16_t usb_receive_size = 0;
uint8_t host_connection = 0;
/* USER CODE END PRIVATE_VARIABLES */

extern USBD_HandleTypeDef hUsbDeviceFS;

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t* pbuf, uint32_t* Len);
static int8_t CDC_TransmitCplt_FS(uint8_t* pbuf, uint32_t* Len, uint8_t epnum);

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS,
    CDC_TransmitCplt_FS,
};

static int8_t CDC_Init_FS(void)
{
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
  return USBD_OK;
}

static int8_t CDC_DeInit_FS(void) { return USBD_OK; }

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
  (void)length;

  switch (cmd) {
    case CDC_SET_CONTROL_LINE_STATE: {
      const uint16_t ctrl_line_state = pbuf[2];
      if (ctrl_line_state == 3U) {
        host_connection = 1U;
      } else {
        host_connection = 0U;
      }
      break;
    }
    default:
      break;
  }

  return USBD_OK;
}

static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t* Len)
{
  (void)Buf;
  usb_receive_size = (uint16_t)(*Len);
  return USBD_OK;
}

uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len)
{
  USBD_CDC_HandleTypeDef* hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
  if (hcdc->TxState != 0U) {
    return USBD_BUSY;
  }
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
  return USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}

static int8_t CDC_TransmitCplt_FS(uint8_t* Buf, uint32_t* Len, uint8_t epnum)
{
  (void)Buf;
  (void)Len;
  (void)epnum;
  usb_transmit_flag = true;
  return USBD_OK;
}
