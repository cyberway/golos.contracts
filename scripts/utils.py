_100percent = 10000
int64_max = 2**63 - 1
uint64_max = 2**64 - 1

fract_digits = {
    'fixp': 12,
    'elaf': 62
}
fixp_max = 2**(63 - fract_digits['fixp']) - 1

import hashlib
from decimal import Decimal
from bson.decimal128 import Decimal128
from config import BALANCE_PRECISION

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
    return get_checked_int64(arg * pow(10, BALANCE_PRECISION))
    
def convert_hash(param):
    a=hashlib.sha256(param.encode('ascii')).hexdigest()[:16]
    q=int(a[14:16] + a[12:14] + a[10:12] + a[8:10] + a[6:8] + a[4:6] + a[2:4] + a[0:2], 16)
    return q if q < 2**63 else -(2**64-q)


def printProgressBar (iteration, total, prefix = '', suffix = '', decimals = 1, length = 100, fill = '█'):
    percent = ("{0:." + str(decimals) + "f}").format(100 * (iteration / float(total)))
    filledLength = int(length * iteration // total)
    bar = fill * filledLength + '-' * (length - filledLength)
    print('\r%s |%s| %s%% %s' % (prefix, bar, percent, suffix), end = '\r')
    if iteration == total:
        print()

def uint_to_hex_str(val, n):
    return '0x' + hex(val)[2:].zfill(n)
     
def char_to_symbol(c):
  if (c >= ord('a') and c <= ord('z')):
    return (c - ord('a')) + 6
  if (c >= ord('1') and c <= ord('5')):
    return (c - ord('1')) + 1;
  return 0;
     
def string_to_name(s):
  len_str = len(s)
  sbytes = str.encode(s)
  value = 0
  
  i = 0
  while (i <= 12):
    c = 0
    
    if (i < len_str and i <= 12):
      c = char_to_symbol(sbytes[i])
    
    if (i < 12):
      c = c & 0x1f
      c = c << 64-5*(i+1)
    else:
      c = c & 0x0f
      
    value = value | c
    i += 1

  return value