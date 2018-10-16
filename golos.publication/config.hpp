#pragma once

// posting bandwith
#define DEFAULT_BANDWIDTH 0
#define MAX_BANDWIDTH 400
#define ONE_BANDWIDTH_PART 100
#define WINDOW 24*60*60
#define POST_AMOUNT_IN_DAY 4
#define PERCENT_DECREASE_REWARD 10

// frequency of operations for post
#define POST_OPERATION_INTERVAL 10 //sec
#define POST_AMOUNT_OPERATIONS 2500 // 25% cost one post
#define POST_EXCESS_COST
#define POST_COST_INCREASE // cost increase for one second recovery

// frequency of operations for comment
#define COMMENT_OPERATION_INTERVAL 20 //sec
#define COMMENT_AMOUNT_OPERATIONS
#define COMMENT_EXCESS_COST
#define COMMENT_COST_INCREASE // cost increase for one second recovery

// frequency of operations for vote
#define VOTE_OPERATION_INTERVAL 3 //sec
#define VOTE_AMOUNT_OPERATIONS
#define VOTE_EXCESS_COST
#define VOTE_COST_INCREASE // cost increase for one second recovery

// post payout window
#define POST_PAYOUT_WINDOW

// percentage of curators
#define FIXED_CURATOR_PERCENT 33
#define RANGE_CURATOR_PERCENT

// max payout share
#define MAX_PAYOUT_SHARE

// penalty window
#define PENALTY_WINDOW 30*60

// closing post params
#define CLOSE_POST_PERIOD 10 // ten seconds for unit_tests (correct value 7*24*60*60)
#define QUNTITY_OF_CHANGES // quantity of changes before post closing

#define UPPER_BOUND (10000) // 100.00%
#define LOWER_BOUND (0) // 0.00%

#define TWO_DECIMAL_DIGITS (100)
#define RECOVERY_PERIOD_POSTING (86400)
#define RECOVERY_PERIOD_VESTING (432000)

#define MAX_REVOTES 5

#define MAX_WEIGHT 10000
