BALANCE_PRECISION = 3
BALANCE_SYMBOL = 'GLS'
VESTING_PRECISION = 6
VESTING_SYMBOL = 'GLS'
BALANCE_SYMBOL_FOR_SCOPE = '......2ndl3k3' # replace ......2ndl3k3 with GLS after closing issue 245

CYBERWAY_PREFIX='_CYBERWAY_'

from utils import fixp_max
_2s = 2 * 2000000000000
golos_curation_func_str = str(fixp_max) + " / ((" + str(_2s) + " / max(x, 0.1)) + 1)"
#see https://github.com/GolosChain/golos/blob/master/libraries/chain/database.cpp: database::get_content_constant_s()
#and ttps://github.com/GolosChain/golos/blob/master/libraries/chain/steem_evaluator.cpp: vote_evaluator::do_apply(...) 
