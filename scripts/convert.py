#!/usr/bin/env python3

import sys
import accounts_and_keys

def print_success():
    print('SUCCESS!')

print('Accounts and authority keys are converting, please waiting: ')
if len(sys.argv) > 1:
    if int(sys.argv[1]):
        accounts_and_keys.convert_accounts_and_keys(sys.argv[1])
        print_success()
    else:
        print('Amount can\'t be equal 0 or negative.')
else:
    accounts_and_keys.convert_accounts_and_keys()
    print_success()

