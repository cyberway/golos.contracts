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
from utils import _100percent

class Struct(object): pass

args = None
logFile = None
golosAccounts = []

unlockTimeout = 999999999

def fillGolosAccounts(golosAccounts):
    _golosAccounts = [
        # name           inGenesis    contract    permissions (name, keys, accounts, links)
        ('gls',          True,       None,
            [("owner",       [], ["gls@witn.smajor"], []),
             ("active",      [], ["gls@witn.smajor", "gls.ctrl@cyber.code", "gls@testing"], []),
             ("golos.io",    ["GLS7KqcmMzmVURzrPvpjfq8ZJSS7Dno82xQ7rpHVYxhz5JnZ9RxY9"], [], []),
             ("testing",     ["GLS5a2eDuRETEg7uy8eHbiCqGZM3wnh2pLjiXrFduLWBKVZKCkB62"], [], []),
             ("issue",       [], ["gls.emit@cyber.code", "gls@testing"], ["cyber.token:issue", "cyber.token:transfer"]),
             ("createuser",  [], ["gls@golos.io"], ["cyber.domain:newusername"]),
             ("invoice",     [], ["gls.charge@cyber.code", "gls.publish@cyber.code"], ["gls.charge:*", "gls.vesting:retire"]),
             ("changevest",  [], ["gls.vesting@cyber.code"], ["gls.ctrl:changevest"]),
             ("providebw",   [], ["gls@golos.io", "gls.emit@cyber.code", "gls.vesting@cyber.code", "gls.publish@cyber.code"], ["cyber:providebw"]},
             ("witn.smajor", [], [], []),
             ("witn.major",  [], [], []),
             ("witn.minor",  [], [], []),
            ]),
        ('gls.ctrl',     True,       'golos.ctrl',
            [("owner",       [], ["gls@owner"], []),
             ("active",      [], ["gls@active"], []),
             ("code",        [], ["gls.ctrl@cyber.code"], ["cyber.token:transfer", "cyber.token:payment", "cyber.token:bulkpayment", "cyber.token:bulktransfer"]),
            ]),
        ('gls.emit',     True,       'golos.emit',
            [("owner",       [], ["gls@owner"], []),
             ("active",      [], ["gls@active"], []),
             ("code",        [], ["gls.emit@cyber.code"], ["gls.emit:emit"]),
            ]),
        ('gls.vesting',  True,       'golos.vesting',
            [("owner",       [], ["gls@owner"], []),
             ("active",      [], ["gls@active"], []),
             ("code",        [], ["gls.vesting@cyber.code"], ["cyber.token:transfer", "cyber.token:payment", "gls.vesting:timeout", "gls.vesting:timeoutconv", "gls.vesting:timeoutrdel"]),
            ]),
        ('gls.publish',  True,       'golos.publication',
            [("owner",       [], ["gls@owner"], []),
             ("active",      [], ["gls@active"], []),
             ("code",        [], ["gls.publish@cyber.code"], ["cyber.token:transfer", "cyber.token:payment", "cyber.token:bulktransfer", "cyber.token:bulkpayment", "gls.publish:closemssgs", "gls.publish:paymssgrwrd", "gls.publish:deletevotes"]),
             ("calcrwrdwt",  [], ["gls.charge@cyber.code"], ["gls.publish:calcrwrdwt"]),
            ]),
        ('gls.social',   True,       'golos.social',
            [("owner",       [], ["gls@owner"], []),
             ("active",      [], ["gls@active"], []),
            ]),
        ('gls.charge',   True,       'golos.charge',
            [("owner",       [], ["gls@owner"], []),
             ("active",      [], ["gls@active"], []),
            ]),
        ('gls.referral', True,       'golos.referral',
            [("owner",       [], ["gls@owner"], []),
             ("active",      [], ["gls@active"], []),
             ("newreferral", [], ["gls@golos.io"], ["gls.referral:addreferral"]),
             ("code",        [], ["gls.referral@cyber.code"]),
            ]),
        ('gls.memo',     True,       None,
            [("owner",       [], ["gls@owner"], []),
             ("active",      [], ["gls@active"], []),
            ]),
        ('gls.worker',   True,       None,
            [("owner",       [], ["gls@owner"], []),
             ("active",      [], ["gls@active"], []),
            ]),
    ]
    
    for (name, inGenesis, contract, permissions) in _golosAccounts:
        acc = Struct()
        perms = []
        for (pname, keys, accounts, links) in permissions:
            perm = Struct()
            parent = "owner" if pname == "active" else "active"
            (perm.name, perm.parent, perm.keys, perm.accounts, perm.links) = (pname, parent, keys, accounts, links)
            perms.append(perm)
        (acc.name, acc.inGenesis, acc.contract, acc.permissions) = (name, inGenesis, contract, perms)
        golosAccounts.append(acc)

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
    retry(args.cleos + 'push action cyber.token transfer ' +
        jsonArg([sender, recipient, amount, memo]) + '-p %s'%sender)

def createAccount(creator, account, key):
    retry(args.cleos + 'create account %s %s %s' % (creator, account, key))

def updateAuth(account, permission, parent, keys, accounts):
    retry(args.cleos + 'push action cyber updateauth' + jsonArg({
        'account': account,
        'permission': permission,
        'parent': parent,
        'auth': createAuthority(keys, accounts)
    }) + '-p ' + account)

def linkAuth(account, code, action, permission):
    if action == '*':
        action = ''
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
    retry(args.cleos + 'push action cyber.token open' +
        jsonArg([account, args.token, account]) + '-p %s'%account)

def issueToken(account, amount):
    retry(args.cleos + 'push action cyber.token issue ' + jsonArg([account, amount, "memo"]) + ' -p gls')

def buyVesting(account, amount):
    issueToken(account, amount)
    transfer(account, 'gls.vesting', amount)   # buy vesting

def registerWitness(ctrl, witness):
    retry(args.cleos + 'push action ' + ctrl + ' regwitness' + jsonArg({
        'witness': witness,
        'url': 'http://%s.witnesses.golos.io' % witness
    }) + '-p %s'%witness)

def voteWitness(ctrl, voter, witness):
    retry(args.cleos + ' push action ' + ctrl + ' votewitness ' +
        jsonArg([voter, witness]) + '-p %s'%voter)

def createPost(author, permlink, header, body, curatorsPrcnt, maxPayout, *, beneficiaries=[]):
    retry(args.cleos + 'push action gls.publish createmssg' +
        jsonArg([author, permlink, "", "", beneficiaries, 0, False, header, body, 'ru', [], '', curatorsPrcnt, maxPayout]) +
        '-p %s'%author)

def createComment(author, permlink, pauthor, ppermlink, header, body, curatorsPrcnt, maxPayout, *, beneficiaries=[]):
    retry(args.cleos + 'push action gls.publish createmssg' +
        jsonArg([author, permlink, pauthor, ppermlink, beneficiaries, 0, False, header, body, 'ru', [], '', curatorsPrcnt, maxPayout]) +
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
    if not args.cyber_private_key in keys:
        run(args.cleos + 'wallet import -n "golos" --private-key ' + args.cyber_private_key)
        keys[args.cyber_private_key] = True
    for a in accounts:
        key = a['pvt']
        if not key in keys:
            if len(keys) >= args.max_user_keys:
                break
            keys[key] = True
            run(args.cleos + 'wallet import -n "golos" --private-key ' + key)

def createGolosAccounts():
    for acc in golosAccounts:
        if not (args.golos_genesis and acc.inGenesis):
            createAccount('cyber', acc.name, args.public_key)
            for perm in acc.permissions:
                updateAuth(acc.name, acc.name, acc.parent, acc.keys, acc.accounts)
                for link in perm.links:
                    (code, action) = link.split(':',2)
                    linkAuth(acc.name, code, action, perm.name)


def stepInstallContracts():
    for acc in golosAccounts:
        if not (args.golos_genesis and acc.inGenesis) and (acc.contract != None):
            retry(args.cleos + 'set contract %s %s' % (acc.name, args.contracts_dir + acc.contract))

def stepCreateTokens():
    if not args.golos_genesis:
        retry(args.cleos + 'push action cyber.token create ' + jsonArg(["gls", intToToken(10000000000*10000)]) + ' -p cyber.token')
    #totalAllocation = allocateFunds(0, len(accounts))
    #totalAllocation = 10000000*10000
    #retry(args.cleos + 'push action cyber.token issue ' + jsonArg(["gls.publish", intToToken(totalAllocation), "memo"]) + ' -p gls.publish')
    if not args.golos_genesis:
        retry(args.cleos + 'push action gls.vesting create ' + jsonArg([args.vesting, 'gls.ctrl']) + '-p gls')
    for acc in golosAccounts:
        openTokenBalance(acc.name)
    sleep(1)

def createCommunity():
    if not args.golos_genesis:
        retry(args.cleos + 'push action gls.emit setparams ' + jsonArg([
            [
                ['inflation_rate',{
                    'start':1500,
                    'stop':95,
                    'narrowing':250000
                }],
                ['reward_pools',{
                    'pools':[
                        {'name':'gls.ctrl','percent':0},
                        {'name':'gls.publish','percent':6000},
                        {'name':'gls.vesting','percent':2400}
                    ]
                }],
                ['emit_token',{
                    'symbol':args.token
                }],
                ['emit_interval',{
                    'value':900
                }]
            ]]) + '-p gls.emit')
        retry(args.cleos + 'push action gls.vesting setparams ' + jsonArg([
            args.vesting,
            [
                ['vesting_withdraw',{
                    'intervals':13,
                    'interval_seconds':120
                }],
                ['vesting_amount',{
                    'min_amount':10000
                }],
                ['vesting_delegation',{
                    'min_amount':5000000,
                    'min_remainder':15000000,
                    'min_time':0,
                    'return_time':120
                }]
            ]]) + '-p gls')
        retry(args.cleos + 'push action gls.ctrl setparams ' + jsonArg([
            [
                ['ctrl_token',{
                    'code':args.symbol
                }],
                ['multisig_acc',{
                    'name':'gls'
                }],
                ['max_witnesses',{
                    'max':5
                }],
                ['multisig_perms',{
                    'super_majority':0,
                    'majority':0,
                    'minority':0
                }],
                ['max_witness_votes',{
                    'max':30
                }],
                ['update_auth',{
                    'period':300
                }]
            ]]) + '-p gls.ctrl')

        retry(args.cleos + 'push action gls.referral setparams ' + jsonArg([[
            ['breakout_parametrs', {'min_breakout': intToToken(10000), 'max_breakout': intToToken(100000)}],
            ['expire_parametrs', {'max_expire': 7*24*3600}],  # sec
            ['percent_parametrs', {'max_percent': 5000}],     # 50.00%
            ['delay_parametrs', {'delay_clear_old_ref': 1*24*3600}] # sec
        ]]) + '-p gls.referral')

def createWitnessAccounts():
    for i in range(firstWitness, firstWitness + numWitness):
        a = accounts[i]
        createAccount('gls', a['name'], a['pub'])
        openVestingBalance(a['name'])
        buyVesting(a['name'], intToToken(10000000*10000))
        registerWitness('gls.ctrl', a['name'])
        voteWitness('gls.ctrl', a['name'], a['name'])

def initCommunity():
    if not args.golos_genesis:
        retry(args.cleos + 'push action gls.publish setparams ' + jsonArg([[
            ['st_max_vote_changes', {'value':5}],
            ['st_cashout_window', {'window': 120, 'upvote_lockout': 15}],
            ['st_max_beneficiaries', {'value': 64}],
            ['st_max_comment_depth', {'value': 127}],
            ['st_social_acc', {'value': 'gls.social'}],
            ['st_referral_acc', {'value': ''}], # TODO Add referral contract to posting-settings
            ['st_curators_prcnt', {'min_curators_prcnt': 0, 'max_curators_prcnt': 9000}]
        ]]) + '-p gls.publish')
    
        retry(args.cleos + 'push action gls.publish setrules ' + jsonArg({
            "mainfunc":{"str":"x","maxarg":fixp_max},
            "curationfunc":{"str":golos_curation_func_str,"maxarg":fixp_max},
            "timepenalty":{"str":"x/1800","maxarg":1800},
            "maxtokenprop":5000,
            "tokensymbol":args.token
        }) + '-p gls.publish')

    # testnet
    limits = [
        # action            charge_id   price                cutoff         vest_price  min_vest
        ("post",            1,          _100percent,         _100percent,   0,          0),
        ("comment",         2,          _100percent/10,      _100percent,   0,          0),
        ("vote",            0,          _100percent/(5*40),  _100percent,   0,          0),
        ("post bandwidth",  3,          _100percent,         _100percent*4, 0,          0),
    ]
    restorers = [
        # charge_id     restorer        max_prev    max_vest    max_elapsed
        (1,             "t/300",        fixp_max,   fixp_max,   fixp_max),
        (2,             "t/200",        fixp_max,   fixp_max,   fixp_max),
        (0,             "t/(5*86400)",  fixp_max,   fixp_max,   fixp_max),
        (3,             "t*p/86400",    fixp_max,   fixp_max,   fixp_max),
    ]

    if not args.golos_genesis:
        for (charge_id, restorer, max_prev, max_vest, max_elapsed) in restorers:
            retry(args.cleos + 'push action gls.charge setrestorer ' + jsonArg({
                "token_code": args.symbol,
                "charge_id": charge_id,
                "func_str": restorer,
                "max_prev": max_prev,
                "max_vesting": max_vest,
                "max_elapsed": max_elapsed
            }) + '-p gls')

    for (action, charge_id, price, cutoff, vest_price, min_vest) in limits:
        retry(args.cleos + 'push action gls.publish setlimit ' + jsonArg({
            "act": action,
            "token_code": args.symbol,
            "charge_id" : charge_id,
            "price": int(price),
            "cutoff": int(cutoff),
            "vesting_price": int(vest_price),
            "min_vesting": int(min_vest)
        }) + '-p gls.publish')

    retry(args.cleos + 'push action gls.emit start ' + jsonArg([]) + '-p gls.emit')

def addUsers():
    for i in range(0, firstWitness-1):
        a = accounts[i]
        createAccount('gls', a['name'], a['pub'])
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
parser.add_argument('--cyber-private-key', metavar='', help="Cyberway Private Key", default='5K463ynhZoCDDa4RDcr63cUwWLTnKqmdcoTKTHBjqoKfv4u5V7p', dest="cyber_private_key")
parser.add_argument('--programs-dir', metavar='', help="Programs directory for cleos, nodeos, keosd", default='/mnt/working/eos-debug.git/build/programs');
parser.add_argument('--cleos', metavar='', help="Cleos command (default in programs-dir)", default='cleos/cleos')
parser.add_argument('--keosd', metavar='', help="Path to keosd binary (default in programs-dir", default='keosd/keosd')
parser.add_argument('--contracts-dir', metavar='', help="Path to contracts directory", default='../../build/')
parser.add_argument('--wallet-dir', metavar='', help="Path to wallet directory", default='./wallet/')
parser.add_argument('--log-path', metavar='', help="Path to log file", default='./output.log')
parser.add_argument('--user-limit', metavar='', help="Max number of users. (0 = no limit)", type=int, default=3000)
parser.add_argument('--max-user-keys', metavar='', help="Maximum user keys to import into wallet", type=int, default=100)
parser.add_argument('--witness-limit', metavar='', help="Maximum number of witnesses. (0 = no limit)", type=int, default=0)
parser.add_argument('--symbol', metavar='', help="The Golos community token symbol", default='GOLOS')
parser.add_argument('--token-precision', metavar='', help="The Golos community token precision", type=int, default=3)
parser.add_argument('--vesting-precision', metavar='', help="The Golos community vesting precision", type=int, default=6)
parser.add_argument('--docker', action='store_true', help='Run actions only for Docker (used with -a)')
parser.add_argument('--golos-genesis', action='store_true', help='Run action only for nodeos with golos-genesis')
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

fillGolosAccounts(golosAccounts)

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
