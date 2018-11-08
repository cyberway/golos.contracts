#!/usr/bin/env python3

import sys
import accounts_and_keys
import balances
import witnesses

accounts_list = [] 

def print_success():
    print('SUCCESS!')

print('Converting in progress, please waiting: ')
if len(sys.argv) == 2:
    if int(sys.argv[1]):
        accounts_list = accounts_and_keys.convert_accounts_and_keys(sys.argv[1])
        balances.convert_balances(accounts_list)
        witnesses.convert_witnesses(accounts_list)
        print_success()
    else:
        print('Amount can\'t be equal 0 or negative.')
elif len(sys.argv) == 1:
    accounts_and_keys.convert_accounts_and_keys()
    balances.convert_balances(accounts_list)
    witnesses.convert_witnesses(accounts_list)
    print_success()
else:
    print('You can\'t set more than 1 argument.')

