#pragma once
#include <common/config.hpp>

namespace golos { namespace config {

const std::string send_prefix = "send to: ";
const int vesting_delay_tx_timeout = 3;     // used to re-create deferred txs. TODO: do not create txs on each block
const uint8_t to_delegated_vesting = 1;
const uint8_t to_delegator = 0;
}} // golos::config
