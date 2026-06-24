#include "raw_can_interface.hpp"

RawCanInterface::RawCanInterface(FDCAN_HandleTypeDef* handler) : handler_(handler) {}

InterfaceStatus RawCanInterface::transmitStandard(uint32_t id, const uint8_t* data, uint8_t dlc) {
  if (handler_ == nullptr || data == nullptr || dlc == 0 || dlc > 8) {
    return InterfaceStatus::INTERFACE_ERROR;
  }

  FDCAN_TxHeaderTypeDef tx_header{};
  tx_header.Identifier = id & 0x7FFU;
  tx_header.IdType = FDCAN_STANDARD_ID;
  tx_header.TxFrameType = FDCAN_DATA_FRAME;
  switch (dlc) {
    case 0: tx_header.DataLength = FDCAN_DLC_BYTES_0; break;
    case 1: tx_header.DataLength = FDCAN_DLC_BYTES_1; break;
    case 2: tx_header.DataLength = FDCAN_DLC_BYTES_2; break;
    case 3: tx_header.DataLength = FDCAN_DLC_BYTES_3; break;
    case 4: tx_header.DataLength = FDCAN_DLC_BYTES_4; break;
    case 5: tx_header.DataLength = FDCAN_DLC_BYTES_5; break;
    case 6: tx_header.DataLength = FDCAN_DLC_BYTES_6; break;
    case 7: tx_header.DataLength = FDCAN_DLC_BYTES_7; break;
    default: tx_header.DataLength = FDCAN_DLC_BYTES_8; break;
  }
  tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  tx_header.BitRateSwitch = FDCAN_BRS_OFF;
  tx_header.FDFormat = FDCAN_CLASSIC_CAN;
  tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  tx_header.MessageMarker = 0;

  uint8_t tx_data[8] = {};
  for (uint8_t i = 0; i < dlc; ++i) {
    tx_data[i] = data[i];
  }

  if (HAL_FDCAN_AddMessageToTxFifoQ(handler_, &tx_header, tx_data) != HAL_OK) {
    return InterfaceStatus::INTERFACE_ERROR;
  }

  return InterfaceStatus::INTERFACE_OK;
}
