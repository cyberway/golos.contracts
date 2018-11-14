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

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
from config import golos_curation_func_str
from utils import fixp_max

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

def cleos(arguments):
    command=args.cleos + arguments
    print('golos-boot-sequence.py:', command)
    logFile.write(command + '\n')
    if subprocess.call(command, shell=True):
        return False
    return True

def run(args):
    print('golos-boot-sequence.py:', args)
    logFile.write(args + '\n')
    if subprocess.call(args, shell=True):
        print('golos-boot-sequence.py: exiting because of error')
        sys.exit(1)

def retry(args):
    count = 5
    while count:
        count = count-1
        print('golos-boot-sequence.py:', args)
        logFile.write(args + '\n')
        if subprocess.call(args, shell=True):
            print('*** Retry: ', count)
            sleep(0.5)
        else:
            return True
    print('golos-boot-sequence.py: exiting because of error')
    sys.exit(1)

def background(args):
    print('golos-boot-sequence.py:', args)
    logFile.write(args + '\n')
    return subprocess.Popen(args, shell=True)

def getOutput(args):
    print('golos-boot-sequence.py:', args)
    logFile.write(args + '\n')
    proc = subprocess.Popen(args, shell=True, stdout=subprocess.PIPE)
    return proc.communicate()[0].decode('utf-8')

def getJsonOutput(args):
    print('golos-boot-sequence.py:', args)
    logFile.write(args + '\n')
    proc = subprocess.Popen(args, shell=True, stdout=subprocess.PIPE)
    return json.loads(proc.communicate()[0])

def sleep(t):
    print('sleep', t, '...')
    time.sleep(t)
    print('resume')

def intToCurrency(value, precision, symbol):
    devider=10**precision
    return '%d.%0*d %s' % (value // devider, precision, value % devider, symbol)

def intToToken(value):
    return intToCurrency(value, args.token_precision, args.symbol)

def intToVesting(value):
    return intToCurrency(value, args.vesting_precision, args.symbol)

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

def linkAuth(account, code, action, permission):
    retry(args.cleos + 'set action permission %s %s %s %s -p %s'%(account, code, action, permission, account))

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

def openVestingBalance(account):
    retry(args.cleos + 'push action gls.vesting open' +
        jsonArg([account, args.vesting, account]) + '-p %s'%account)

def openTokenBalance(account):
    retry(args.cleos + 'push action eosio.token open' +
        jsonArg([account, args.token, account]) + '-p %s'%account)

def issueToken(account, amount):
    retry(args.cleos + 'push action eosio.token issue ' + jsonArg([account, amount, "memo"]) + ' -p gls.publish')

def buyVesting(account, amount):
    issueToken(account, amount)
    transfer(account, 'gls.vesting', amount)   # buy vesting

def registerWitness(ctrl, witness, key):
    retry(args.cleos + 'push action ' + ctrl + ' regwitness' + jsonArg({
        'domain': args.token,
        'witness': witness,
        'key': key,
        'url': 'http://%s.witnesses.golos.io' % witness
    }) + '-p %s'%witness)

def voteWitness(ctrl, voter, witness, weight):
    retry(args.cleos + ' push action ' + ctrl + ' votewitness ' + 
        jsonArg([args.token, voter, witness, weight]) + '-p %s'%voter)

def createPost(author, permlink, header, body, *, beneficiaries=[]):
    retry(args.cleos + 'push action gls.publish createmssg' + 
        jsonArg([author, permlink, "", "", beneficiaries, 0, False, header, body, 'ru', [], '']) +
        '-p %s'%author)

def createComment(author, permlink, pauthor, ppermlink, header, body, *, beneficiaries=[]):
    retry(args.cleos + 'push action gls.publish createmssg' + 
        jsonArg([author, permlink, pauthor, ppermlink, beneficiaries, 0, False, header, body, 'ru', [], '']) +
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

def importKeys():
    run(args.cleos + 'wallet create -n "golos" --to-console')
    keys = {}
    run(args.cleos + 'wallet import -n "golos" --private-key ' + args.private_key)
    keys[args.private_key] = True
    if not args.eosio_private_key in keys:
        run(args.cleos + 'wallet import -n "golos" --private-key ' + args.eosio_private_key)
        keys[args.eosio_private_key] = True
    for a in accounts:
        key = a['pvt']
        if not key in keys:
            if len(keys) >= args.max_user_keys:
                break
            keys[key] = True
            run(args.cleos + 'wallet import -n "golos" --private-key ' + key)

def createGolosAccounts():
    for a in golosAccounts:
        createAccount('eosio', a, args.public_key)
        openTokenBalance(a)
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
    retry(args.cleos + 'push action eosio.token create ' + jsonArg(["gls.publish", intToToken(10000000000*10000)]) + ' -p eosio.token')
    #totalAllocation = allocateFunds(0, len(accounts))
    #totalAllocation = 10000000*10000 
    #retry(args.cleos + 'push action eosio.token issue ' + jsonArg(["gls.publish", intToToken(totalAllocation), "memo"]) + ' -p gls.publish')
    retry(args.cleos + 'push action gls.vesting createvest ' + jsonArg(['gls.publish', args.vesting, golosAccounts]) + '-p gls.publish')
    sleep(1)

def createCommunity():
    control = 'gls.ctrl'
    owner = 'gls.publish'
    retry(args.cleos + 'push action ' + control + ' create ' + jsonArg({
        'domain': args.token,
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
        buyVesting(a['name'], intToToken(10000000*10000))
        registerWitness('gls.ctrl', a['name'], a['pub'])
        voteWitness('gls.ctrl', a['name'], a['name'], 10000)
        
def initCommunity():
    retry(args.cleos + 'push action gls.emit start ' + jsonArg([args.token]) + '-p gls.publish')
    retry(args.cleos + 'push action gls.publish setrules ' + jsonArg({
        "mainfunc":{"str":"x","maxarg":fixp_max},
        "curationfunc":{"str":golos_curation_func_str,"maxarg":fixp_max},
        "timepenalty":{"str":"x/1800","maxarg":1800},
        "curatorsprop":2500,
        "maxtokenprop":5000,
        "tokensymbol":args.token,
        "lims":{
            "restorers":["t / 3600"],
            "limitedacts":[
                {"chargenum":0, "restorernum": 0, "cutoffval":20000, "chargeprice":9900},
                {"chargenum":0, "restorernum": 0, "cutoffval":30000, "chargeprice":1000},
                {"chargenum":1, "restorernum": 0, "cutoffval":10000, "chargeprice":1000},
                {"chargenum":0, "restorernum":-1, "cutoffval":10000, "chargeprice":   0}
            ],
            "vestingprices":[-1, -1],
            "minvestings":[0, 0, 0]
        }
    }) + '-p gls.publish')

def addUsers():
    for i in range(0, firstWitness-1):
        a = accounts[i]
        createAccount('eosio', a['name'], a['pub'])
        openVestingBalance(a['name'])
        buyVesting(a['name'], intToToken(50000))

# Command Line Arguments

parser = argparse.ArgumentParser()

commands = [
#    Short Command          Function                    inAll  inDocker Description
    ('k', 'kill',           stepKillAll,                True,  False,   "Kill all nodeos and keosd processes"),
    ('w', 'wallet',         startWallet,                True,  False,   "Start wallet (start keosd)"),
    ('K', 'keys',           importKeys,                 True,  True,    "Create and fill wallet with keys"),
    ('A', 'accounts',       createGolosAccounts,        True,  True,    "Create golos accounts (gls.*)"),
    ('c', 'contracts',      stepInstallContracts,       True,  True,    "Install contracts (ctrl, emit, vesting, publish)"),
    ('t', 'tokens',         stepCreateTokens,           True,  True,    "Create tokens"),
    ('C', 'community',      createCommunity,            True,  True,    "Create community"),
    ('U', 'witnesses',      createWitnessAccounts,      True,  True,    "Create witnesses accounts"),
    ('i', 'init',           initCommunity,              True,  True,    "Init community"),
    ('u', 'users',          addUsers,                   True,  True,    "Add users"),
]

parser.add_argument('--public-key', metavar='', help="Golos Public Key", default='GLS6Tvw3apAGeHCUTWpf9DY4xvUoKwmuDatW7GV8ygkuZ8F6y4Yor', dest="public_key")
parser.add_argument('--private-key', metavar='', help="Golos Private Key", default='5KekbiEKS4bwNptEtSawUygRb5sQ33P6EUZ6c4k4rEyQg7sARqW', dest="private_key")
parser.add_argument('--eosio-private-key', metavar='', help="EOSIO Private Key", default='5K463ynhZoCDDa4RDcr63cUwWLTnKqmdcoTKTHBjqoKfv4u5V7p', dest="eosio_private_key")
parser.add_argument('--programs-dir', metavar='', help="Programs directory for cleos, nodeos, keosd", default='/mnt/working/eos-debug.git/build/programs');
parser.add_argument('--cleos', metavar='', help="Cleos command (default in programs-dir)", default='cleos/cleos')
parser.add_argument('--keosd', metavar='', help="Path to keosd binary (default in programs-dir", default='keosd/keosd')
parser.add_argument('--contracts-dir', metavar='', help="Path to contracts directory", default='../../build/')
parser.add_argument('--wallet-dir', metavar='', help="Path to wallet directory", default='./wallet/')
parser.add_argument('--log-path', metavar='', help="Path to log file", default='./output.log')
parser.add_argument('--user-limit', metavar='', help="Max number of users. (0 = no limit)", type=int, default=3000)
parser.add_argument('--max-user-keys', metavar='', help="Maximum user keys to import into wallet", type=int, default=100)
parser.add_argument('--witness-limit', metavar='', help="Maximum number of witnesses. (0 = no limit)", type=int, default=0)
parser.add_argument('--symbol', metavar='', help="The Golos community token symbol", default='GLS')
parser.add_argument('--token-precision', metavar='', help="The Golos community token precision", type=int, default=6)
parser.add_argument('--vesting-precision', metavar='', help="The Golos community vesting precision", type=int, default=6)
parser.add_argument('--docker', action='store_true', help='Run actions only for Docker (used with -a)')
parser.add_argument('-a', '--all', action='store_true', help="Do everything marked with (*)")
parser.add_argument('-H', '--http-port', type=int, default=8000, metavar='', help='HTTP port for cleos')

for (flag, command, function, inAll, inDocker, help) in commands:
    prefix = ''
    if inAll or inDocker: prefix += ('*' if inAll else ' ') + ('D' if inDocker else ' ')
    if prefix: help = '(' + prefix + ') ' + help
    if flag:
        parser.add_argument('-' + flag, '--' + command, action='store_true', help=help, dest=command)
    else:
        parser.add_argument('--' + command, action='store_true', help=help, dest=command)
        
args = parser.parse_args()

if (parser.get_default('cleos') == args.cleos):
    args.cleos = args.programs_dir + '/' + args.cleos
    args.cleos += ' --wallet-url http://127.0.0.1:6667 --url http://127.0.0.1:%d ' % args.http_port

if (parser.get_default('keosd') == args.keosd):
    args.keosd = args.programs_dir + '/' + args.keosd

args.token = '%d,%s' % (args.token_precision, args.symbol)
args.vesting = '%d,%s' % (args.vesting_precision, args.symbol)

logFile = open(args.log_path, 'a')

logFile.write('\n\n' + '*' * 80 + '\n\n\n')

accounts_filename = os.path.dirname(os.path.realpath(__file__)) + '/accounts.json'
with open(accounts_filename) as f:
    a = json.load(f)
    if args.user_limit:
        del a['users'][args.user_limit:]
    if args.witness_limit:
        del a['witnesses'][args.witness_limit:]
    firstWitness = len(a['users'])
    numWitness = len(a['witnesses'])
    accounts = a['users'] + a['witnesses']

haveCommand = False
for (flag, command, function, inAll, inDocker, help) in commands:
    if getattr(args, command) or (inDocker if args.docker else inAll) and args.all:
        if function:
            haveCommand = True
            function()
if not haveCommand:
    print('golos-boot-sequence.py: Tell me what to do. -a does almost everything. -h shows options.')
