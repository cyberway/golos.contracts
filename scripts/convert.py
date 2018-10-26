#!/usr/bin/env python3

import accounts
import authority_keys

def print_success():
    print('SUCCESS!')

print('Accounts are converting, please waiting: ')
if accounts.convert_accounts():
    print_success()

print('Authority keys are converting, please waiting: ')
if authority_keys.convert_authority_keys():
    print_success()
