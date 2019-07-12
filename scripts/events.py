#!/usr/bin/python3

import json
import fileinput


def printAsset(asset):
    #decs = 10 ** asset['decs']
    #return "%.*f %s" % (asset['decs'], asset['amount']/decs, asset['sym'])
    return asset


def cyberToken_issue(action):
    args = action['args']
    return "(%s) ---> %s  // %s" % (printAsset(args['quantity']), args['to'], args['memo'])

def cyberToken_transfer(action):
    args = action['args']
    return "%s ---(%s)---> %s  // %s" % (args['from'], printAsset(args['quantity']), args['to'], args['memo'])


def cyberToken_currency(event):
    args = event['args']
    return "supply: %s, max_supply: %s" % (printAsset(args['supply']), printAsset(args['max_supply']))

def cyberToken_balance(event):
    args = event['args']
    return "%s:  %s" % (args['account'], printAsset(args['balance']))

def glsPublish_createmssg(action):
    args = action['args']
    parent = "%s/%s" % (args['parent_id']['author'], args['parent_id']['permlink']) if args['parent_id']['author'] else args['parent_id']['permlink']
    return "%s/%s (%s)    '%s'" % (args['message_id']['author'], args['message_id']['permlink'], parent, args['headermssg'])

def glsPublish_closemssgs(action):
    return ""

def glsPublish_upvote(action):
    args = action['args']
    return "%s   %s/%s %.2f%%" % (args['voter'], args['message_id']['author'], args['message_id']['permlink'], args['weight']/100)

def glsPublish_downvote(action):
    args = action['args']
    return "%s   %s/%s %.2f%%" % (args['voter'], args['message_id']['author'], args['message_id']['permlink'], args['weight']/100)

def glsPublish_unvote(action):
    args = action['args']
    return "%s   %s/%s" % (args['voter'], args['message_id']['author'], args['message_id']['permlink'])


def glsPublish_poolState(event):
    args = event['args']
    return "%d:  %s  %d" % (args['created'], printAsset(args['funds']), args['rshares'])

def glsPublish_postState(event):
    args = event['args']
    return "%s/%s  %d" % (args['author'], args['permlink'], args['voteshares'])

def glsPublish_postClose(event):
    args = event['args']
    return "%s/%s  %d" % (args['author'], args['permlink'], args['rewardweight'])

def glsPublish_voteState(event):
    args = event['args']
    return "%s %s/%s  %d" % (args['voter'], args['author'], args['permlink'], args['rshares'])

def glsCtrl_changeVest(action):
    args = action['args']
    return "%s  diff: %s" % (args['who'], printAsset(args['diff']))

def glsCtrl_witnessState(event):
    args = event['args']
    return "%s  %d" % (args['witness'], args['weight'])

def eosio_updateAuth(action):
    args = action['args']
    auth = "%d " % args['auth']['threshold']
    for account in args['auth']['accounts']:
        auth += " %s@%s/%d" % (account['permission']['actor'], account['permission']['permission'], account['weight'])
    return "%s@%s %s" % (args['account'], args['permission'], auth)

_actions = (
    ('cyber.token',   None,    'issue',    cyberToken_issue),
    ('cyber.token',   None,    'transfer', cyberToken_transfer),
    ('gls.publish',   None,    'createmssg', glsPublish_createmssg),
    ('gls.publish',   None,    'closemssgs',  glsPublish_closemssgs),
    ('gls.publish',   None,    'upvote',     glsPublish_upvote),
    ('gls.publish',   None,    'downvote',   glsPublish_downvote),
    ('gls.publish',   None,    'unvote',     glsPublish_unvote),
    ('gls.ctrl',      None,    'changevest', glsCtrl_changeVest),
    ('eosio',         None,    'updateauth', eosio_updateAuth),
)

_events = (
    ('cyber.token',   None,    'currency',   cyberToken_currency),
    ('cyber.token',   None,    'balance',    cyberToken_balance),
    ('gls.publish',   None,    'poolstate',  glsPublish_poolState),
    ('gls.publish',   None,    'poststate',  glsPublish_postState),
    ('gls.publish',   None,    'votestate',  glsPublish_voteState),
    ('gls.publish',   None,    'postclose',  glsPublish_postClose),
    ('gls.ctrl',      None,    'witnessstate', glsCtrl_witnessState),
)

class Contract:
    def __init__(self):
        self.actions = {}
        self.events = {}

    def addAction(self, code, action, func):
        self.actions[code+':'+action] = func

    def addEvent(self, code, event, func):
        self.events[code+':'+event] = func

    def processAction(self, action):
        func = self.actions.get(action['code']+':'+action['action'], None)
        return func(action) if func else None
        if func:
            return func(action)

    def processEvent(self, event):
        func = self.events.get(event['code']+':'+event['event'], None)
        return func(event) if func else None

actions = {}
for (code, receiver, action, func) in _actions:
    name = receiver if receiver else code
    contract = actions.get(name, None)
    if contract:
        contract.addAction(code, action, func)
    else:
        contract = Contract()
        contract.addAction(code, action, func)
        actions[name] = contract

for (code, receiver, event, func) in _events:
    name = receiver if receiver else code
    contract = actions.get(name, None)
    if contract:
        contract.addEvent(code, event, func)
    else:
        contract = Contract()
        contract.addEvent(code, event, func)
        actions[name] = contract

trxAccepts = {}

def acceptTrx(msg):
    trxAccepts[msg['id']] = msg['accepted']


def applyTrx(msg):
    result = '?'
    if msg['id'] in trxAccepts:
        result = "OK" if trxAccepts[msg['id']] else "FAIL"
    print("----------- Apply transaction in block %d:  %s  (%-4s)" % (msg['block_num'], msg['id'], result))
    for action in msg['actions']:
        contract = actions.get(action['receiver'])
        header = "%-13s  %s:%s"%(action['receiver'], action['code'], action['action'])
        data = None
        if contract:
            data = contract.processAction(action) if action['args'] else "CAN'T PARSE ARGS"
        print("   %-40s  %s" % (header, data if data else ""))
        for event in action['events']:
            hdr = "%15c   %s:%s"%(' ', event['code'], event['event'])
            if contract:
                data = contract.processEvent(event)
            print("   %-43s  %s" % (hdr, data if data else ""))

def acceptBlock(msg):
    print("================= Accept block: %d ===================" % msg['block_num'])

def commitBlock(msg):
    pass

process_funcs = {'AcceptTrx': acceptTrx,
                 'ApplyTrx': applyTrx,
                 'AcceptBlock': acceptBlock,
                 'CommitBlock': commitBlock}

def process_line(line):
    if line[0] != '{':
        return
    try:
        msg = json.loads(line)
    except:
        print(line)
        exit(1)
    #print("Msg: %s" % msg['msg_type'])
    func = process_funcs.get(msg['msg_type'], None)
    if func != None:
        func(msg)

for line in fileinput.input():
    process_line(line)
