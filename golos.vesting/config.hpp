#pragma once
#include <common/config.hpp>

namespace golos { namespace config {

const int vesting_delay_tx_timeout = 3;     // used to re-create deferred txs. TODO: do not create txs on each block
}} // golos::config
