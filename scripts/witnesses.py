import dbs
import bson

golos_witnesses = dbs.golos_db['witness_object']
golos_witnesses_votes = dbs.golos_db['witness_vote_object']

regex = "^[a-z]*([\.]|[a-z]|[1-5])*[a-o\.]$"
acc_list = []

def record_votes(account, witnesses_list):
    witnessvote = {
        "community" : "blog", 
        "witnesses" : witnesses_list, 
        "_SCOPE_" : account, 
        "_PAYER_" : account, 
        "_SIZE_" : bson.Int64(41) 
    }
    if dbs.cyberway_gls_ctrl_db['witnessvote'].find({"_SCOPE_":account}).count():
        dbs.cyberway_gls_ctrl_db['witnessvote'].update({"_SCOPE_":account}, witnessvote)
    else:
        dbs.cyberway_gls_ctrl_db['witnessvote'].save(witnessvote)

def save_witness_votes(doc, accounts_list = []):
    witnesses_list = []
    if "removed" in doc.keys():
        return

    if len(accounts_list):
        if dbs.cyberway_gls_ctrl_db['witnessvote'].find({"_SCOPE_":doc["account"]}).count():
            for acc in dbs.cyberway_gls_ctrl_db['witnessvote'].find({"_SCOPE_":doc["account"]}):
                witnesses_list = acc["witnesses"]
                witnesses_list.append(doc["witness"])
                record_votes(doc["account"], witnesses_list)
        else:
            witnesses_list.append(doc["witness"])
            record_votes(doc["account"], witnesses_list)
    else:
        for account in acc_list:
            if account == doc["account"]:
                return
        acc_list.append(doc["account"])
        for obj in golos_witnesses_votes.find({"account":doc["account"],
            "witness":{"$regex":regex}, "$expr": { "$lte": [ { "$strLenCP": "$witness" }, 12 ]}}):
            witnesses_list.append(obj["witness"])
        record_votes(doc["account"], witnesses_list)

def save_witness(doc):
    if dbs.cyberway_gls_ctrl_db['witness'].find({"name":doc["owner"]}).count():
        return
    if doc["signing_key"] == "GLS1111111111111111111111111111111114T1Anm":
        return
    witness = { 
        "name" : doc["owner"],
        "key" : doc["signing_key"],
        "url" : doc["url"],
        "active" : True,
        "total_weight" : doc["votes"],
        "_SCOPE_" : doc["owner"], 
        "_PAYER_" : doc["owner"], 
        "_SIZE_" : bson.Int64(61) 
    }
    dbs.cyberway_gls_ctrl_db['witness'].save(witness)

def convert_witnesses(accounts_list):
    if len(accounts_list):
        for account in accounts_list:
            for doc in golos_witnesses.find({"owner":account}):
                save_witness(doc)
            for doc in golos_witnesses_votes.find({"witness":account}):
                save_witness_votes(doc, accounts_list)
    else:
        for doc in golos_witnesses.find({"owner":{"$regex":regex},
            "$expr": { "$lte": [ { "$strLenCP": "$owner" }, 12 ]}}):
            save_witness(doc)
        for doc in golos_witnesses_votes.find({"account":{"$regex":regex}, 
            "$expr": { "$lte": [ { "$strLenCP": "$account" }, 12 ]},
            "witness":{"$regex":regex},
            "$expr": { "$lte": [ { "$strLenCP": "$witness" }, 12 ]}}):
            save_witness_votes(doc)

