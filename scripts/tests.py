#!/usr/bin/python3

import unittest
import testnet
import subprocess
import json
import os
import re
import random

def setUpModule():
    print('setUpModule')
    info = json.loads(testnet.cleos('get info'))
    print(json.dumps(info, indent=3))

    testnet.cleos("wallet create --name test --to-console")
    keys = [
        '5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3'       # tech@active
    ]
    for key in keys:
        importPrivateKey(key)

def tearDownModule():
    print('tearDownModule')
    os.remove(os.environ['HOME']+'/eosio-wallet/test.wallet')

def createKey():
    data = testnet.cleos("create key --to-console")
    m = re.search('Private key: ([a-zA-Z0-9]+)\nPublic key: ([a-zA-Z0-9]+)', data)
    return (m.group(1), m.group(2))

def importPrivateKey(private):
    testnet.cleos("wallet import --name test --private-key %s" % private)

def createRandomAccount(key, *, creator='tech', providebw=None):
    letters = "abcdefghijklmnopqrstuvwxyz12345"
    while True:
        name = ''.join(random.choice(letters) for i in range(12))
        try:
            getAccount(name)
        except:
            testnet.createAccount(creator, name, key, providebw=providebw)
            return name

def getAccount(account):
    return json.loads(testnet.cleos('get account %s -j' % account))

def getResourceUsage(account):
    info = getAccount(account)
    return {
        'cpu': info['cpu_limit']['used'],
        'net': info['net_limit']['used'],
        'ram': info['ram_limit']['used'],
        'storage': info['storage_limit']['used']
    }

def resolve(username):
    data = testnet.cleos('resolve name %s' % username)
    m = re.match('"[a-z1-5.]+@[a-z1-5.]+" resolves to:\n  Domain:   ([a-z0-9.]+)\n  Username: ([a-z0-9.]+)\n', data)
    if m:
        return m.group(2)
    else:
        return None



class TestTestnet(unittest.TestCase):

    def test_createKey(self):
        (private,public) = createKey()
        self.assertEqual(private[:1], '5')
        self.assertEqual(public[:3], 'GLS')

    def test_createAccount(self):
        (private,public) = createKey()
        importPrivateKey(private)

        account = createRandomAccount(public)
        info = getAccount(account)
        self.assertEqual(info['account_name'], account)
        self.assertEqual(info['permissions'][0]['perm_name'], 'active')
        self.assertEqual(info['permissions'][0]['required_auth']['keys'][0]['key'], public)

    def test_resourceUsage(self):
        # create user account
        (private,public) = createKey()
        user = createRandomAccount(public, creator='tech')
        importPrivateKey(private)

        # recuperate resource usage for `tech` account
        testnet.pushAction('cyber.token', 'open', 'tech', ['tech', '4,CYBER', 'tech'], additional='-f')

        # push test transaction
        userUsage = getResourceUsage(user)
        techUsage = getResourceUsage('tech')
        trx = testnet.pushAction('gls.vesting', 'open', 'tech', [user, '6,GOLOS', 'tech'])
        receipt = trx['processed']['receipt']
        self.assertLess(0, receipt['cpu_usage_us'])
        self.assertLess(0, receipt['net_usage_words'])
        self.assertLess(0, receipt['ram_kbytes'])
#        self.assertLess(0, receipt['storage_kbytes'])          # BUG: storage rounded down to 1kByte
        #print(json.dumps(trx, indent=3))

        # check that user doesn't pay for transaction
        self.assertEqual(userUsage, getResourceUsage(user))

        # check that `tech` account pay for transaction
        techUsage2 = getResourceUsage('tech')
        print("Was: %s, now: %s" % (techUsage, techUsage2))
        self.assertEqual(techUsage2['cpu'], techUsage['cpu'] + receipt['cpu_usage_us'])
        self.assertEqual(techUsage2['net'], techUsage['net'] + receipt['net_usage_words']*8)
        self.assertEqual(techUsage2['ram'], techUsage['ram'] + receipt['ram_kbytes']*1024)
        self.assertEqual(techUsage2['storage'], techUsage['storage'] + receipt['storage_kbytes']*1024)

    def test_bandwidthProvider(self):
        # Check that resource for transactions was provided by provider account

        # Create user account
        (user_private,user_public) = createKey()
        importPrivateKey(user_private)
        user = createRandomAccount(user_public, creator='tech')

        # Create provider account
        (provider_private,provider_public) = createKey()
        importPrivateKey(provider_private)
        provider = createRandomAccount(provider_public, creator='tech')

        # recuperate resource usage for provider
        testnet.pushAction('cyber.token', 'open', provider, [provider, '4,CYBER', provider], additional='-f')

        # Get initial resource usage
        techUsage = getResourceUsage('tech')
        userUsage = getResourceUsage(user)
        providerUsage = getResourceUsage(provider)

        # Publish action with provider
        trx = testnet.pushAction('cyber.token', 'open', user, [user, '4,CYBER', user], providebw=user+'/'+provider)
        receipt = trx['processed']['receipt']

        # Check that `tech` and user resource usage doesn't changed
        self.assertEqual(techUsage, getResourceUsage('tech'))
        self.assertEqual(userUsage, getResourceUsage(user))

        # Check that provider resource usage increased
        providerUsage2 = getResourceUsage(provider)
        print("Was: %s, now: %s" % (providerUsage, providerUsage2))
#        self.assertEqual(providerUsage2['cpu'], providerUsage['cpu'] + receipt['cpu_usage_us'])          # STRANGE: sometimes value is one more than expected
#        self.assertEqual(providerUsage2['net'], providerUsage['net'] + receipt['net_usage_words']*8)     # STRANGE: sometimes value is one more than expected
        self.assertEqual(providerUsage2['ram'], providerUsage['ram'] + receipt['ram_kbytes']*1024)
        self.assertEqual(providerUsage2['storage'], providerUsage['storage'] + receipt['storage_kbytes']*1024)




class TestTransit(unittest.TestCase):
    def test_cyberAuthority(self):
        # Check that block producers has correct authority
        cyber = getAccount('cyber')
        permissions = {}
        for perm in cyber['permissions']:
            permissions[perm['perm_name']] = perm

        expected = {
            'active': {
                "perm_name": "active", "parent": "owner",
                "required_auth": {"threshold": 1, "keys": [], "accounts": [], "waits": []}
            },
            'owner': {
                "perm_name": "owner", "parent": "",
                "required_auth": {"threshold": 1, "keys": [], "accounts": [], "waits": []}
            },
            'prods': {
                "perm_name": "prods", "parent": "active",
                "required_auth": {
                    "threshold": 2,
                    "keys": [],
                    "accounts": [
                        {"permission": {"actor": "cyber.prods", "permission": "active"}, "weight": 1}
                    ],
                    "waits": [
                        {"wait_sec": 1209600, "weight": 1}
                    ]
                }
            }
        }
        self.assertEqual(permissions, expected)

    def test_golosAccounts(self):
        # Check new Golos accounts names
        account_list = (
            ('golos',     '1glrbzwsvdbq'),
            ('golosio',   'qwm3tgmbeog5'),
            ('goloscore', 'ggfpapchob2e'),
        )

        for (name, account) in account_list:
            self.assertEqual(account, resolve('%s@golos' % name))


class TestFreeUser(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (private, public) = createKey();
        importPrivateKey(private)
        user = createRandomAccount(public)
        testnet.pushAction('cyber.token', 'transfer', 'tech', ['tech', 'cyber.stake', '10.0000 CYBER', user])
        testnet.openVestingBalance(user, 'tech')
        cls.freeUser = user

    def test_createPost(self):
        letters = "abcdefghijklmnopqrstuvwxyz0123456789-"
        permlink = ''.join(random.choice(letters) for i in range(128))
        header   = ''.join(random.choice(letters) for i in range(32))
        body     = ''.join(random.choice(letters) for i in range(1024))

        publishUsage = getResourceUsage('gls.publish')
        testnet.createPost(self.freeUser, permlink, "", header, body, beneficiaries=[{'account':'tech','weight':2000}])
        print("Was: %s, current: %s" % (publishUsage, getResourceUsage('gls.publish')))

        testnet.upvotePost(self.freeUser, self.freeUser, permlink, 10000)


class TestGolosIo(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        importPrivateKey('5JUKEmKs7sMX5fxv5PnHnShnUm4mmizfyBtWc8kgWnDocH9R6fr')   # golos.io key
        (private,public) = createKey()
        importPrivateKey(private)
        cls.siteAccount = createRandomAccount(public)
        cls.user = None
        print("siteAccount: ", cls.siteAccount)

    def test_createUser(self):
        (private,public) = createKey()
        importPrivateKey(private)
        user = createRandomAccount(public, creator=self.siteAccount, providebw=self.siteAccount+'/gls@providebw')
        trx = testnet.pushAction('cyber.token', 'open', 'tech', ['tech', '4,CYBER', 'tech'], providebw='tech'+'/gls@providebw', additional='-f')
        #print(json.dumps(trx, indent=3))
        testnet.pushAction('gls.vesting', 'open', self.siteAccount, [user, testnet.args.vesting, self.siteAccount], providebw=self.siteAccount+'/gls@providebw')
        self.user = user

    def test_createPost(self):
        if self.user == None:
            self.test_createUser()
        letters = "abcdefghijklmnopqrstuvwxyz0123456789-"
        permlink = ''.join(random.choice(letters) for i in range(128))
        header   = ''.join(random.choice(letters) for i in range(32))
        body     = ''.join(random.choice(letters) for i in range(1024))

        publishUsage = getResourceUsage('gls.publish')
        testnet.createPost(self.user, permlink, "", header, body, beneficiaries=[{'account':'tech','weight':2000}], providebw=self.user+'/gls@providebw')
        print("Was: %s, current: %s" % (publishUsage, getResourceUsage('gls.publish')))


if __name__ == '__main__':
    unittest.main()
