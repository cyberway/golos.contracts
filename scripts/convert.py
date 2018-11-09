#!/usr/bin/env python3

import sys
import accounts

def print_success():
    print('SUCCESS!')

print('Converting in progress, please waiting: ')
if len(sys.argv) == 2:
    count = int(sys.argv[1])
    if count <= 0:
        print('Amount can\'t be equal 0 or negative.')
elif len(sys.argv) == 1:
    count = -1
else:
    print('You can\'t set more than 1 argument.')

accounts.convert_accounts(count)
print_success()
