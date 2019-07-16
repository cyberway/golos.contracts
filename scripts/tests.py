#!/usr/bin/python3

import unittest
import testnet
import subprocess
import json
import os
import re
import random

golosIoKey='5JUKEmKs7sMX5fxv5PnHnShnUm4mmizfyBtWc8kgWnDocH9R6fr'
testingKey='5JdhhMMJdb1KEyCatAynRLruxVvi7mWPywiSjpLYqKqgsT4qjsN'
techKey   ='5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3'

def randomUsername():
    letters = 'abcdefghijklmnopqrstuvwxyz0123456789'
    return ''.join(random.choice(letters) for i in range(16))

def randomPermlink():
    letters = "abcdefghijklmnopqrstuvwxyz0123456789-"
    return ''.join(random.choice(letters) for i in range(128))

def randomText(length):
    letters = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()_+-=;:,.<>/?\\~`"
    return ''.join(random.choice(letters) for i in range(length))

def setUpModule():
    print('setUpModule')
    info = json.loads(testnet.cleos('get info'))
    print(json.dumps(info, indent=3))

    testnet.cleos("wallet create --name test --to-console")
    for key in [techKey]:
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

def createRandomAccount(key, *, creator='tech', providebw=None, keys=None):
    letters = "abcdefghijklmnopqrstuvwxyz12345"
    while True:
        name = ''.join(random.choice(letters) for i in range(12))
        try:
            getAccount(name)
        except:
            break
    testnet.createAccount(creator, name, key, providebw=providebw, keys=keys)
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

    @unittest.skip("Incorrect check of consumed resources")
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
        self.assertGreaterEqual(techUsage2['storage'], techUsage['storage'] +  receipt['storage_kbytes']   *1024)
        self.assertLessEqual   (techUsage2['storage'], techUsage['storage'] + (receipt['storage_kbytes']+1)*1024)

    @unittest.skip("Resource delegation required")
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
            self.assertEqual(account, resolve(name+'@golos'))


class TestFreeUser(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (private, public) = createKey();
        importPrivateKey(private)
        user = createRandomAccount(public)
        testnet.pushAction('cyber.token', 'transfer', 'tech', ['tech', 'cyber.stake', '100.0000 CYBER', user])
        testnet.openVestingBalance(user, 'tech')
        cls.freeUser = user

    def test_createPost(self):
        permlink = randomPermlink()
        publishUsage = getResourceUsage('gls.publish')

        # Check that user can post using own bandwidth
        testnet.createPost(self.freeUser, permlink, "", randomText(32), randomText(1024))
        print("Was: %s, current: %s" % (publishUsage, getResourceUsage('gls.publish')))

        # Check that user can upvote using own bandwidth
        testnet.upvotePost(self.freeUser, self.freeUser, permlink, 10000)

class TestGolosIo(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (private,public) = createKey()
        cls.siteKey = private
        cls.siteAccount = createRandomAccount(public)
        print("siteAccount: ", cls.siteAccount)

    def test_createUser(self):
        (private,public) = createKey()
        username = randomUsername()
        
        user = createRandomAccount(
                public, creator=self.siteAccount, 
                providebw=self.siteAccount+'/gls@providebw',
                keys=[self.siteKey, golosIoKey])

        testnet.pushAction(
                'gls.vesting', 'open', self.siteAccount, [user, testnet.args.vesting, self.siteAccount], 
                providebw=self.siteAccount+'/gls@providebw',
                keys=[self.siteKey, golosIoKey])

        testnet.pushAction(
                'cyber.token', 'open', self.siteAccount, [user, testnet.args.token, self.siteAccount],
                providebw=self.siteAccount+'/gls@providebw',
                keys=[self.siteKey, golosIoKey])

        testnet.pushAction(
                'cyber.domain', 'newusername', 'gls@createuser', ['gls', user, username],
                keys=[golosIoKey])

    def test_createPost(self):
        (private,public) = createKey()
        user = createRandomAccount(public)
        testnet.pushAction('gls.vesting', 'open', 'tech', [user, testnet.args.vesting, 'tech'])

        permlink = randomPermlink()
        publishUsage = getResourceUsage('gls.publish')
        testnet.createPost(
                user, permlink, "", randomText(32), randomText(1024), 
                providebw=user+'/gls@providebw', 
                keys=[private, golosIoKey])
        print("Was: %s, current: %s" % (publishUsage, getResourceUsage('gls.publish')))

        testnet.upvotePost(
                user, user, permlink, 10000, 
                providebw=user+'/gls@providebw', 
                keys=[private, golosIoKey])

    def test_addReferral(self):
        params = json.loads(testnet.cleos('get table gls.referral gls.referral refparams'))['rows'][0]

        (private1,public1) = createKey()
        user1 = createRandomAccount(public1)

        (private2,public2) = createKey()
        user2 = createRandomAccount(public2)

        percent = params['percent_params']['max_percent']
        breakout = params['breakout_params']['min_breakout']
        expire = 60
        testnet.pushAction(
                'gls.referral', 'addreferral', 'gls.referral@newreferral', 
                [user1, user2, percent, expire, breakout],
                providebw='gls.referral/gls@providebw',
                keys=[golosIoKey])

    def test_doesntAllowedChangeParams(self):
        # Check that golos.io can't change parameters without leaders
        params = json.loads(testnet.cleos('get table gls.vesting GOLOS vestparams'))['rows'][0]
        min_amount = params['min_amount']['min_amount']
        min_amount += -1 if min_amount%2 else +1
        
        with self.assertRaisesRegex(Exception, 'transaction declares authority \'{"actor":"gls","permission":"active"}\', but does not have signatures for it'):
            testnet.pushAction(
                    'gls.vesting', 'setparams', 'gls@active', ["6,GOLOS",[["vesting_amount",{"min_amount":min_amount}]]],
                    keys=[golosIoKey])

        with self.assertRaisesRegex(Exception, 'action declares irrelevant authority \'{"actor":"gls","permission":"golos.io"}\'; minimum authority is {"actor":"gls","permission":"active"}'):
            testnet.pushAction(
                    'gls.vesting', 'setparams', 'gls@golos.io', ["6,GOLOS",[["vesting_amount",{"min_amount":min_amount}]]],
                    keys=[golosIoKey])

    def test_emitCallByAny(self):
        (private,public) = createKey()
        user = createRandomAccount(public)

        try:
            testnet.pushAction('gls.emit', 'emit', user, [],
                    providebw=user+'/tech@active',
                    keys=[private,techKey])
        except Exception as err:
            self.assertRegex(str(err), 'assertion failure with message: emit called too early')

    def test_closemssgsCallByAny(self):
        (private,public) = createKey()
        user = createRandomAccount(public)

        testnet.pushAction('gls.publish', 'closemssgs', user, [user],
                providebw=user+'/tech@active',
                keys=[private,techKey])

    def test_paymssgrwrdCallByAny(self):
        (private,public) = createKey()
        author = createRandomAccount(public)
        testnet.openVestingBalance(author, 'tech', providebw=author+'/tech', keys=[techKey])

        (private2,public2) = createKey()
        user = createRandomAccount(public2)

        permlink = randomPermlink()
        testnet.createPost(
                author, permlink, "", randomText(32), randomText(1024), 
                providebw=author+'/tech@active', 
                keys=[private, techKey])

        with self.assertRaisesRegex(Exception, "Message doesn't closed"):
            testnet.pushAction(
                    'gls.publish', 'paymssgrwrd', user, {'message_id':{'author':author,'permlink':permlink}},
                    providebw=user+'/tech@active',
                    keys=[private2, techKey])

    def test_timeoutCallByAny(self):
        (private,public) = createKey()
        user = createRandomAccount(public)

        try:
            testnet.pushAction('gls.vesting', 'timeout', user, ["6,GOLOS"],
                    providebw=[user+'/tech@active', 'gls.vesting/tech@active'],
                    keys=[private, techKey])
            self.fail('Timeout does not scheduled')
        except Exception as err:
            self.assertRegex(str(err), 'deferred transaction with the same sender_id and payer already exists')

        testnet.pushAction('gls.vesting', 'timeoutconv', user, [],
                providebw=[user+'/tech@active', 'gls.ctrl/tech@active'],
                keys=[private, techKey])

        testnet.pushAction('gls.vesting', 'timeoutrdel', user, [],
                providebw=user+'/tech@active',
                keys=[private, techKey])

    def test_closeoldrefCallByAny(self):
        (private,public) = createKey()
        user = createRandomAccount(public)

        testnet.pushAction('gls.referral', 'closeoldref', user, [],
                providebw=user+'/tech@active',
                keys=[private, techKey])



class TestGolosIoTesting(unittest.TestCase):
    def test_provideGolosToken(self):
        (priate,public) = createKey()
        user = createRandomAccount(public)

        # We need use issue to self and then transfer due error:
        #   Error 3100006: Subjective exception thrown during block production
        #   Error Details:
        #   Authorization failure with inline action sent to self
        #   transaction declares authority '{"actor":"gls","permission":"active"}', but does not have signatures for it under a provided delay of 0 ms, provided permissions [{"actor":"cyber.token","permission":"cyber.code"}], provided keys [], and a delay max limit of 3888000000 ms
        amount = '100.000 GOLOS'
        testnet.pushAction('cyber.token', 'issue', 'gls@issue', ['gls', amount, ''],
                keys=[testingKey])

        testnet.pushAction('cyber.token', 'transfer', 'gls@issue', ['gls', user, amount, ''],
                keys=[testingKey])

        self.assertEqual(amount+'\n', testnet.cleos('get currency balance cyber.token %s GOLOS' % user))

    def test_setVestingParams(self):
        params = json.loads(testnet.cleos('get table gls.vesting GOLOS vestparams'))['rows'][0]
        min_amount = params['min_amount']['min_amount']
        min_amount += -1 if min_amount%2 else +1
        testnet.pushAction(
                'gls.vesting', 'setparams', 'gls', ["6,GOLOS",[["vesting_amount",{"min_amount":min_amount}]]],
                keys=[testingKey])

    def test_setPostingParams(self):
        params = json.loads(testnet.cleos('get table gls.publish gls.publish pstngparams'))['rows'][0]
        max_comment_depth = params['max_comment_depth']['value']
        max_comment_depth += -1 if max_comment_depth%2 else +1
        testnet.pushAction(
                'gls.publish', 'setparams', 'gls.publish', [[["st_max_comment_depth",{"value":max_comment_depth}]]],
                providebw='gls.publish/gls@providebw',
                keys=[testingKey, golosIoKey])


if __name__ == '__main__':
    unittest.main()
