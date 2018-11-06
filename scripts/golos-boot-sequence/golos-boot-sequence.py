#!/usr/bin/env python3

import argparse
import json
import numpy
import os
import random
import re
import subprocess
import sys
import time

args = None
logFile = None

unlockTimeout = 999999999

golosAccounts = [
    'gls.ctrl',
    'gls.emit',
    'gls.vesting',
    'gls.publish',
    'gls.worker',
]

def jsonArg(a):
    return " '" + json.dumps(a) + "' "

def run(args):
    print('golos-boot-tutorial.py:', args)
    logFile.write(args + '\n')
    if subprocess.call(args, shell=True):
        print('golos-boot-tutorial.py: exiting because of error')
        sys.exit(1)

def retry(args):
    count = 5
    while count:
        count = count-1
        print('golos-boot-tutorial.py:', args)
        logFile.write(args + '\n')
        if subprocess.call(args, shell=True):
            print('*** Retry: ', count)
        else:
            break

def background(args):
    print('golos-boot-tutorial.py:', args)
    logFile.write(args + '\n')
    return subprocess.Popen(args, shell=True)

def getOutput(args):
    print('golos-boot-tutorial.py:', args)
    logFile.write(args + '\n')
    proc = subprocess.Popen(args, shell=True, stdout=subprocess.PIPE)
    return proc.communicate()[0].decode('utf-8')

def getJsonOutput(args):
    print('golos-boot-tutorial.py:', args)
    logFile.write(args + '\n')
    proc = subprocess.Popen(args, shell=True, stdout=subprocess.PIPE)
    return json.loads(proc.communicate()[0])

def sleep(t):
    print('sleep', t, '...')
    time.sleep(t)
    print('resume')

def intToCurrency(i):
    return '%d.%04d %s' % (i // 10000, i % 10000, args.symbol)

# --------------------- EOSIO functions ---------------------------------------

def transfer(sender, recipient, amount, memo=""):
    retry(args.cleos + 'push action eosio.token transfer ' +
        jsonArg([sender, recipient, amount, memo]) + '-p %s'%sender)

def createAccount(creator, account, key):
    retry(args.cleos + 'create account %s %s %s' % (creator, account, key))

def updateAuth(account, permission, parent, keys, accounts):
    retry(args.cleos + 'push action eosio updateauth' + jsonArg({
        'account': account,
        'permission': permission,
        'parent': parent,
        'auth': createAuthority(keys, accounts)
    }) + '-p ' + account)

def createAuthority(keys, accounts):
    keys.sort()
    accounts.sort()
    keysList = []
    accountsList = []
    for k in keys:
        keysList.extend([{'weight':1,'key':k}])
    for a in accounts:
        d = a.split('@',2)
        if len(d) == 1:
            d.extend(['active'])
        accountsList.extend([{'weight':1,'permission':{'actor':d[0],'permission':d[1]}}])
    return {'threshold': 1, 'keys': keysList, 'accounts': accountsList, 'waits':[]}

# --------------------- GOLOS functions ---------------------------------------

def registerWitness(ctrl, witness, key, symbol):
    retry(args.cleos + 'push action ' + ctrl + ' regwitness' + jsonArg({
        'domain': '4,%s' % symbol,
        'witness': witness,
        'key': key,
        'url': 'http://%s.witnesses.golos.io' % witness
    }) + '-p %s'%witness)

def voteWitness(ctrl, voter, witness, weight, symbol):
    retry(args.cleos + ' push action ' + ctrl + ' votewitness ' + 
        jsonArg(['4,%s' % symbol, voter, witness, weight]) + '-p %s'%voter)

def createPost(author, permlink, header, body):
    retry(args.cleos + 'push action gls.publish createmssg' + 
        jsonArg([author, permlink, "", "", [], 0, False, header, body, 'ru', [], '']) +
        '-p %s'%author)

def upvotePost(voter, author, permlink, weight):
    retry(args.cleos + 'push action gls.publish upvote' + 
        jsonArg([voter, author, permlink, weight]) +
        '-p %s'%voter)

def downvotePost(voter, author, permlink, weight):
    retry(args.cleos + 'push action gls.publish downvote' + 
        jsonArg([voter, author, permlink, weight]) +
        '-p %s'%voter)

def unvotePost(voter, author, permlink):
    retry(args.cleos + 'push action gls.publish unvote' + 
        jsonArg([voter, author, permlink]) +
        '-p %s'%voter)

# --------------------- ACTIONS -----------------------------------------------

def stepKillAll():
    run('killall keosd || true')
    sleep(1.5)

def startWallet():
    run('rm -rf ' + os.path.abspath(args.wallet_dir))
    run('mkdir -p ' + os.path.abspath(args.wallet_dir))
    background(args.keosd + ' --unlock-timeout %d --http-server-address 127.0.0.1:6667 --unix-socket-path \'\' --wallet-dir %s' % (unlockTimeout, os.path.abspath(args.wallet_dir)))
    sleep(.4)
    run(args.cleos + 'wallet create --to-console')

def importKeys():
    run(args.cleos + 'wallet import --private-key ' + args.eosio_private_key)
    run(args.cleos + 'wallet import --private-key ' + args.private_key)
    keys = {}
    for a in accounts:
        key = a['pvt']
        if not key in keys:
            if len(keys) >= args.max_user_keys:
                break
            keys[key] = True
            run(args.cleos + 'wallet import --private-key ' + key)

def createGolosAccounts():
    for a in golosAccounts:
        createAccount('eosio', a, args.public_key)
    updateAuth('gls.publish', 'witn.major', 'active', [args.public_key], [])
    updateAuth('gls.publish', 'witn.minor', 'active', [args.public_key], [])
    updateAuth('gls.publish', 'active', 'owner', [args.public_key], ['gls.ctrl@eosio.code'])
    updateAuth('gls.vesting', 'active', 'owner', [args.public_key], ['gls.vesting@eosio.code'])
    updateAuth('gls.emit',    'active', 'owner', [args.public_key], ['gls.emit@eosio.code'])

def stepInstallContracts():
    retry(args.cleos + 'set contract gls.ctrl ' + args.contracts_dir + 'golos.ctrl/')
    retry(args.cleos + 'set contract gls.emit ' + args.contracts_dir + 'golos.emit/')
    retry(args.cleos + 'set contract gls.vesting ' + args.contracts_dir + 'golos.vesting/')
    retry(args.cleos + 'set contract gls.publish ' + args.contracts_dir + 'golos.publication/')

def stepCreateTokens():
    retry(args.cleos + 'push action eosio.token create \'["gls.publish", "10000000000.0000 %s"]\' -p eosio.token' % (args.symbol))
    #totalAllocation = allocateFunds(0, len(accounts))
    totalAllocation = 10000000*10000 
    retry(args.cleos + 'push action eosio.token issue \'["gls.publish", "%s", "memo"]\' -p gls.publish' % intToCurrency(totalAllocation))
    retry(args.cleos + 'push action gls.vesting createvest ' + jsonArg(['gls.publish', '4,%s'%args.symbol,golosAccounts]) + '-p gls.publish')
    sleep(1)

def createCommunity():
    control = 'gls.ctrl'
    owner = 'gls.publish'
    symbol = '4,%s' % args.symbol
    retry(args.cleos + 'push action ' + control + ' create ' + jsonArg({
        'domain': symbol,
        'owner': owner,
        'props': {
            'max_witnesses': 5,
            'witness_supermajority': 0,
            'witness_majority': 0,
            'witness_minority': 0,
            'max_witness_votes': 30,
            'infrate_start': 1500,
            'infrate_stop': 95,
            'infrate_narrowing': 250000*6,
            'content_reward': 6667-667,
            'vesting_reward': 2667-267,
            'workers_reward': 1000,
            'workers_pool': 'gls.worker',
        }
    }) + '-p ' + owner)

def createWitnessAccounts():
    for i in range(firstWitness, firstWitness + numWitness):
        a = accounts[i]
        createAccount('eosio', a['name'], a['pub'])
        transfer('gls.publish', a['name'], '1000.0000 %s'%args.symbol)
        transfer(a['name'], 'gls.vesting', '100.0000 %s'%args.symbol)   # buy vesting
        registerWitness('gls.ctrl', a['name'], a['pub'], args.symbol)
        voteWitness('gls.ctrl', a['name'], a['name'], 10000, args.symbol)
        
def initCommunity():
    retry(args.cleos + 'push action gls.emit start ' + jsonArg(['4,%s'%args.symbol]) + '-p gls.publish')
    retry(args.cleos + 'push action gls.publish setrules ' + jsonArg({
        "mainfunc":{"str":"0","maxarg":1},
        "curationfunc":{"str":"0","maxarg":1},
        "timepenalty":{"str":"0","maxarg":1},
        "curatorsprop":0,
        "maxtokenprop":0,
        "tokensymbol":"4,%s"%args.symbol,
        "lims":{
            "restorers":["1"],
            "limitedacts":[
                {"chargenum":0, "restorernum":0, "cutoffval":0, "chargeprice":0},
                {"chargenum":0, "restorernum":0, "cutoffval":0, "chargeprice":0},
                {"chargenum":0, "restorernum":0, "cutoffval":0, "chargeprice":0},
                {"chargenum":0, "restorernum":0, "cutoffval":0, "chargeprice":0}
            ],
            "vestingprices":[0, 0],
            "minvestings":[0, 0, 0]
        }
    }) + '-p gls.publish')

def addUsers():
    for i in range(0, firstWitness-1):
        a = accounts[i]
        createAccount('eosio', a['name'], a['pub'])
        retry(args.cleos + 'push action gls.vesting open' +
            jsonArg([a['name'], '4,%s'%args.symbol, a['name']]) + '-p %s'%a['name'])

# Command Line Arguments

parser = argparse.ArgumentParser()

commands = [
    ('k', 'kill',           stepKillAll,                True,    "Kill all nodeos and keosd processes"),
    ('w', 'wallet',         startWallet,                True,    "Start keosd, create wallet"),
    ('K', 'keys',           importKeys,                 True,    "Fill wallet with keys"),
    ('A', 'accounts',       createGolosAccounts,        True,    "Create golos accounts (gls.*)"),
    ('c', 'contracts',      stepInstallContracts,       True,    "Install contracts (ctrl, emit, vesting, publish)"),
    ('t', 'tokens',         stepCreateTokens,           True,    "Create tokens"),
    ('C', 'community',      createCommunity,            True,    "Create community"),
    ('d', 'witnesses',      createWitnessAccounts,      True,    "Create witnesses accounts"),
    ('i', 'init',           initCommunity,              True,    "Init community"),
    ('u', 'users',          addUsers,                   True,    "Add users"),
]

parser.add_argument('--public-key', metavar='', help="Golos Public Key", default='GLS6Tvw3apAGeHCUTWpf9DY4xvUoKwmuDatW7GV8ygkuZ8F6y4Yor', dest="public_key")
parser.add_argument('--private-Key', metavar='', help="Golos Private Key", default='5KekbiEKS4bwNptEtSawUygRb5sQ33P6EUZ6c4k4rEyQg7sARqW', dest="private_key")
parser.add_argument('--eosio-private-Key', metavar='', help="EOSIO Private Key", default='5K463ynhZoCDDa4RDcr63cUwWLTnKqmdcoTKTHBjqoKfv4u5V7p', dest="eosio_private_key")
parser.add_argument('--programs-dir', metavar='', help="Programs directory for cleos, nodeos, keosd", default='/mnt/working/eos-debug.git/build/programs');
parser.add_argument('--cleos', metavar='', help="Cleos command (default in programs-dir)", default='cleos/cleos')
parser.add_argument('--keosd', metavar='', help="Path to keosd binary (default in programs-dir", default='keosd/keosd')
parser.add_argument('--contracts-dir', metavar='', help="Path to contracts directory", default='../../build/')
parser.add_argument('--wallet-dir', metavar='', help="Path to wallet directory", default='./wallet/')
parser.add_argument('--log-path', metavar='', help="Path to log file", default='./output.log')
parser.add_argument('--symbol', metavar='', help="The Golos community symbol", default='GLS')
parser.add_argument('--user-limit', metavar='', help="Max number of users. (0 = no limit)", type=int, default=3000)
parser.add_argument('--max-user-keys', metavar='', help="Maximum user keys to import into wallet", type=int, default=100)
parser.add_argument('--witness-limit', metavar='', help="Maximum number of witnesses. (0 = no limit)", type=int, default=0)
parser.add_argument('-a', '--all', action='store_true', help="Do everything marked with (*)")
parser.add_argument('-H', '--http-port', type=int, default=8000, metavar='', help='HTTP port for cleos')

for (flag, command, function, inAll, help) in commands:
    prefix = ''
    if inAll: prefix += '*'
    if prefix: help = '(' + prefix + ') ' + help
    if flag:
        parser.add_argument('-' + flag, '--' + command, action='store_true', help=help, dest=command)
    else:
        parser.add_argument('--' + command, action='store_true', help=help, dest=command)
        
args = parser.parse_args()

if (parser.get_default('cleos') == args.cleos):
    args.cleos = args.programs_dir + '/' + args.cleos

if (parser.get_default('keosd') == args.keosd):
    args.keosd = args.programs_dir + '/' + args.keosd

args.cleos += ' --wallet-url http://127.0.0.1:6667 --url http://127.0.0.1:%d ' % args.http_port

logFile = open(args.log_path, 'a')

logFile.write('\n\n' + '*' * 80 + '\n\n\n')

with open('accounts.json') as f:
    a = json.load(f)
    if args.user_limit:
        del a['users'][args.user_limit:]
    if args.witness_limit:
        del a['witnesses'][args.witness_limit:]
    firstWitness = len(a['users'])
    numWitness = len(a['witnesses'])
    accounts = a['users'] + a['witnesses']

haveCommand = False
for (flag, command, function, inAll, help) in commands:
    if getattr(args, command) or inAll and args.all:
        if function:
            haveCommand = True
            function()
if not haveCommand:
    print('golos-boot-tutorial.py: Tell me what to do. -a does almost everything. -h shows options.')
