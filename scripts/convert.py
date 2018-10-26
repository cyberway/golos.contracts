#!/usr/bin/env python3

import accounts_and_keys

print('Accounts and authority keys are converting, please waiting: ')
if accounts_and_keys.convert_accounts_and_keys():
    print('SUCCESS!')
