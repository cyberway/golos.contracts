_100percent = 10000
int64_max = 2**63 - 1
uint64_max = 2**64 - 1

fract_digits = {
    'fixp': 12,
    'elaf': 62
}
golos_decs = 3

import hashlib
from decimal import Decimal
from bson.decimal128 import Decimal128

def get_checked_int64(arg):
    if arg > int64_max:
        raise OverflowError('get_checked_int64 error', arg)
    return int(arg);
    
def uint64_to_int64(arg):
    if arg > uint64_max:
        raise OverflowError('uint64_to_int64 error', arg)
    return arg if arg <= int64_max else -(2**64 - arg)

def get_fixp_raw(arg):
    return get_checked_int64(arg * pow(2, fract_digits['fixp']))

def get_prop_raw(arg):
    return get_checked_int64((arg * pow(2, fract_digits['elaf'])) / _100percent)
    
def get_golos_asset_amount(arg):
    return get_checked_int64(arg * pow(10, golos_decs))
    
def convert_hash(param): #TODO
    hashobj = hashlib.sha256(param.encode('utf-8')).hexdigest()
    return Decimal128(Decimal(int(hashobj[:16], 16))) 

def printProgressBar (iteration, total, prefix = '', suffix = '', decimals = 1, length = 100, fill = '█'):
    percent = ("{0:." + str(decimals) + "f}").format(100 * (iteration / float(total)))
    filledLength = int(length * iteration // total)
    bar = fill * filledLength + '-' * (length - filledLength)
    print('\r%s |%s| %s%% %s' % (prefix, bar, percent, suffix), end = '\r')
    if iteration == total:
        print()
