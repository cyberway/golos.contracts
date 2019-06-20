#!/usr/bin/env python3
import os
import sys
import subprocess

default_contracts_dir = '/opt/cyberway/bin/data-dir/contracts/'
nodeos_url = os.environ.get('CYBERWAY_URL', 'http://nodeosd:8888')

extra_args=''
if not os.environ.get('NO_GENESIS', False):
    extra_args += " --golos-genesis"

args = {
    'basedir': os.path.dirname(os.path.dirname(os.path.realpath(__file__))),
    'cleos':'/opt/cyberway/bin/cleos --url=%s ' % nodeos_url,
    'cyber_public_key':'GLS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV',
    'cyber_private_key':'5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3',
    'golos_public_key':'GLS5a2eDuRETEg7uy8eHbiCqGZM3wnh2pLjiXrFduLWBKVZKCkB62',
    'golos_private_key':'5JdhhMMJdb1KEyCatAynRLruxVvi7mWPywiSjpLYqKqgsT4qjsN',
    'cyberway_contracts_dir': os.environ.get('CYBERWAY_CONTRACTS', default_contracts_dir),
    'golos_contracts_dir': os.environ.get('GOLOS_CONTRACTS', default_contracts_dir),
    'extra_args': extra_args,
}


if os.environ.get('NO_GENESIS', False):
    bios_boot_sequence=('{basedir}/cyberway.contracts/scripts/bios-boot-sequence/bios-boot-sequence.py '
                         '--cleos "{cleos}" --contracts-dir "{cyberway_contracts_dir}" '
                         '--public-key {cyber_public_key} --private-key {cyber_private_key} '
                         '--symbol=CYBER '
                         '--docker --all {extra_args}').format(**args)
    if subprocess.call(bios_boot_sequence, shell=True):
        print('bios-boot-sequence.py exited with error')
        sys.exit(1)

golos_boot_sequence=('{basedir}/scripts/golos-boot-sequence/golos-boot-sequence.py '
                     '--cleos "{cleos}" --contracts-dir "{golos_contracts_dir}" ' 
                     '--public-key {golos_public_key} --private-key {golos_private_key} '
                     '--cyber-private-key {cyber_private_key} '
                     '--docker --all {extra_args}').format(**args)
if subprocess.call(golos_boot_sequence, shell=True):
    print('golos-boot-sequence.py exited with error')
    sys.exit(1)

sys.exit(0)
