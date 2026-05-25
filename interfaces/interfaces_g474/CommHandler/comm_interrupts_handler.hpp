/*
 * comm_interrupts_handler.hpp
 *
 * Interrupt-driven read/write helper for CommInterface (USB, UART, etc.).
 */

#ifndef SRC_INTERFACES_G474_COMM_HANDLER_COMM_INTERRUPTS_HANDLER_HPP_
#define SRC_INTERFACES_G474_COMM_HANDLER_COMM_INTERRUPTS_HANDLER_HPP_

#include <memory>
#include <vector>

#include "comm_interface.hpp"
#include "interface_status.hpp"

class CommInterruptsHandler {
  const size_t buffer_size_;
  std::shared_ptr<CommInterface> comm_;

 public:
  CommInterruptsHandler(std::shared_ptr<CommInterface> comm, const size_t buffer_size)
      : buffer_size_(buffer_size), comm_(std::move(comm)) {}

  InterfaceStatus write(uint8_t* tx_buffer, std::vector<uint8_t>& pending_tx_buffer, const uint8_t* data = nullptr,
                        const size_t size = 0);
  InterfaceStatus read(uint8_t* rx_buffer, uint8_t* data, uint16_t& size);
  InterfaceStatus read(uint8_t* rx_buffer, uint8_t* data);

  std::shared_ptr<CommInterface> comm() const { return comm_; }
};

#endif /* SRC_INTERFACES_G474_COMM_HANDLER_COMM_INTERRUPTS_HANDLER_HPP_ */
