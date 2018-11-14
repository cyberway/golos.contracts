BALANCE_PRECISION = 4
BALANCE_SYMBOL = 'GLS'
VESTING_PRECISION = 4
VESTING_SYMBOL = 'GLS'

CYBERWAY_PREFIX='_CYBERWAY_'

from utils import fixp_max
_2s = 2 * 12000000000000
golos_curation_func_str = str(fixp_max) + " / ((" + str(_2s) + " / max(x, 0.1)) + 1)"
