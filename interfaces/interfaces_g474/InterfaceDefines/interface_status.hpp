/*
 * interface_status.hpp
 *
 *  Created on: Jul 10, 2024
 *      Author: matan
 */

#ifndef SRC_INTERFACE_STATUS_HPP_
#define SRC_INTERFACE_STATUS_HPP_

#include <stdint.h>

enum InterfaceStatus : uint8_t {
  INTERFACE_OK = 0x00U,
  INTERFACE_ERROR = 0x01U,
  INTERFACE_BUSY = 0x02U,
  INTERFACE_TIMEOUT = 0x03U
};

#endif /* SRC_INTERFACE_STATUS_HPP_ */
