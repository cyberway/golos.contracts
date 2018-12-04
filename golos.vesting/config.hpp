#pragma once
#include <common/config.hpp>

namespace golos { namespace config {

const std::string send_prefix = "send to: ";
const int vesting_delay_tx_timeout = 3;     // used to re-create deferred txs. TODO: do not create txs on each block

const struct vesting_withdraw {
    const int intervals = 13;                           // VESTING_WITHDRAW_INTERVALS
    const int interval_seconds = 120;   // 60*60*24*7   // VESTING_WITHDRAW_INTERVAL_SECONDS
    const int min_amount = 10 * 1e3;    // acc fee * 10
} vesting_withdraw;


const struct delegation {
    const int min_amount    = 5e6;        // min step. TODO: must somehow be derived from precision
    const int min_remainder = 15e6;
    const int min_time      = 0;
    const int max_interest  = 0;
    const int return_time   = 120;      // 60*60*24*7 = cashout time
} delegation;

}} // golos::config
