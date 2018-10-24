#!/usr/bin/env python3

import accounts
import authority_keys

def print_success():
    print('SUCCESS!')

print('Accounts converting: ')
if accounts.convert_accounts():
    print_success()

print('Authority converting: ')
if authority_keys.convert_authority_keys():
    print_success()
