import dbs
import bson
from config import *
import utils

golos = dbs.Tables([
    ('witnesses', dbs.golos_db['witness_object'],      None, [('owner',True)]),
    ('votes',     dbs.golos_db['witness_vote_object'], None, [('account',True)])
])

cyberway = dbs.Tables([
    ('account',     dbs.cyberway_db['account'],              dbs.get_next_id, [('name',True)]),
    ('vesting',     dbs.cyberway_gls_vesting_db['accounts'], None,            None),
    ('witness',     dbs.cyberway_gls_ctrl_db['witness'],     None,            None),
    ('witnessvote', dbs.cyberway_gls_ctrl_db['witnessvote'], None,            None)
])

def create_witness(doc, exist_accounts):
    total_weight = 0
    acc_list = []

    for vote in golos.votes.table.find({"witness":doc["owner"]}):
        if "removed" in vote.keys():
            continue
        if not vote["account"] in exist_accounts:
            continue
        if vote["account"] in acc_list:
            continue
        acc_list.append(vote["account"])
    for acc in acc_list:
        total_weight += int(cyberway.vesting.table.find_one({"_SERVICE_.scope":acc})["vesting"]["amount"].to_decimal())
        
    witness = { 
        "name" : doc["owner"],
        "key" : doc["signing_key"],
        "url" : doc["url"],
        "active" : True,
        "total_weight" : utils.UInt64(total_weight),
        "_SERVICE_" : {
            "scope" : "gls.ctrl",
            "rev" : utils.Int64(1),
            "payer" : doc["owner"],
            "size" : 61
        }
    }
    cyberway.witness.append(witness)

def create_vote(account, witnesses_list):
    witnessvote = {
        "community" : "blog", 
        "witnesses" : witnesses_list, 
        "_SERVICE_" : {
            "scope" : account,
            "rev" : utils.Int64(1),
            "payer" : account,
            "size" : 41
        }
    }
    cyberway.witnessvote.append(witnessvote)

def convert_witnesses():
    exist_accounts = {}
    for acc in cyberway.account.table.find(projection={'name':1}):
        exist_accounts[acc["name"]] = True
    
    witnesses_list = []
    for doc in cyberway.account.table.find(projection={'name':1}):
        witness = golos.witnesses.table.find_one({"owner":doc["name"], "signing_key":{"$ne":"GLS1111111111111111111111111111111114T1Anm"}})
        if witness:
            create_witness(witness, exist_accounts)
        for vote in golos.votes.table.find({"account":doc["name"]}):
            if "removed" in vote.keys():
                continue
            if not vote["witness"] in exist_accounts:
                continue
            if vote["witness"] in witnesses_list:
                continue
            witnesses_list.append(vote["witness"])
        if len(witnesses_list):
            create_vote(doc["name"], witnesses_list)
    cyberway.writeCache()

