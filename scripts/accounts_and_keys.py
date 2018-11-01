import dbs
import bson

golos_authority_keys = dbs.golos_db['account_authority_object']
cyberway_permusage = dbs.cyberway_db['permusage']
cyberway_permission = dbs.cyberway_db['permission']

def save_authority(doc):
    if (cyberway_permission.find({"owner":doc["account"], "name":"owner"}).count() or 
            cyberway_permission.find({"owner":doc["account"], "name":"active"}).count() or 
            cyberway_permission.find({"owner":doc["account"], "name":"posting"}).count()):
        return

    default_id_count_permusage = cyberway_permusage.find().sort('id', -1).limit(1)[0]['id']
    id_count_permusage = default_id_count_permusage + 1
    id_count_permission = cyberway_permission.find().sort('id', -1).limit(1)[0]['id'] + 1

    owner_permusage = {
	"id" : bson.Int64(id_count_permusage),
	"last_used" : doc["last_owner_update"].isoformat(),
	"_SCOPE_" : "",
	"_PAYER_" : "",
	"_SIZE_" : bson.Int64(16)
    }
    dbs.cyberway_db['permusage'].save(owner_permusage)
    id_count_permusage += 1

    owner_permission = {
	"id" : bson.Int64(id_count_permission),
	"usage_id" : owner_permusage["id"],
	"parent" : bson.Int64(0),
	"owner" : doc["account"],
	"name" : "owner",
	"last_updated" : doc["last_owner_update"].isoformat(),
	"auth" : {
	    "threshold" : bson.Int64(1),
	    "keys" : [
		{
		    "key" : doc["owner"][0],
		    "weight" : bson.Int64(1)
		}
	    ],
	    "accounts" : [ ],
	    "waits" : [ ]
	},
	"_SCOPE_" : "",
	"_PAYER_" : "",
	"_SIZE_" : bson.Int64(91)
    }
    dbs.cyberway_db['permission'].save(owner_permission)
    id_count_permission += 1

    active_permusage = {
	"id" : bson.Int64(id_count_permusage),
	"last_used" : doc["last_owner_update"].isoformat(),
	"_SCOPE_" : "",
	"_PAYER_" : "",
	"_SIZE_" : bson.Int64(16)
    }
    dbs.cyberway_db['permusage'].save(active_permusage)
    id_count_permusage += 1

    active_permission = {
	"id" : bson.Int64(id_count_permission),
	"usage_id" : active_permusage["id"],
	"parent" : bson.Int64(id_count_permission-1),
	"owner" : doc["account"],
	"name" : "active",
	"last_updated" : doc["last_owner_update"].isoformat(),
	"auth" : {
	    "threshold" : bson.Int64(1),
	    "keys" : [
		{
		    "key" : doc["active"][0],
		    "weight" : bson.Int64(1)
		}
	    ],
	    "accounts" : [ ],
	    "waits" : [ ]
	},
	"_SCOPE_" : "",
	"_PAYER_" : "",
	"_SIZE_" : bson.Int64(91)
    }
    dbs.cyberway_db['permission'].save(active_permission)
    id_count_permission += 1

    posting_permusage = {
	"id" : bson.Int64(id_count_permusage),
	"last_used" : doc["last_owner_update"].isoformat(),
	"_SCOPE_" : "",
	"_PAYER_" : "",
	"_SIZE_" : bson.Int64(16)
    }
    dbs.cyberway_db['permusage'].save(posting_permusage)
    id_count_permusage += 1
        
    posting_permission = {
	"id" : bson.Int64(id_count_permission),
	"usage_id" : posting_permusage["id"],
	"parent" : bson.Int64(id_count_permission-1),
	"owner" : doc["account"],
	"name" : "posting",
	"last_updated" : doc["last_owner_update"].isoformat(),
	"auth" : {
	    "threshold" : bson.Int64(1),
	    "keys" : [
		{
		    "key" : doc["posting"][0],
		    "weight" : bson.Int64(1)
		}
	    ],
	    "accounts" : [ ],
	    "waits" : [ ]
	},
	"_SCOPE_" : "",
	"_PAYER_" : "",
	"_SIZE_" : bson.Int64(91)
    }
    dbs.cyberway_db['permission'].save(posting_permission)
    id_count_permission += 1


def convert_accounts_and_keys(argv = -1):
    golos_accounts = dbs.golos_db['account_object']
    cyberway_accounts = dbs.cyberway_db['account']
    
    default_id_count = cyberway_accounts.find().sort('id', -1).limit(1)[0]['id']
    id_count = default_id_count + 1

    accounts_list = []

    for doc in golos_accounts.find({"name":{"$regex":"^[a-z]*([\.]|[a-z]|[1-5])*[a-o\.]$"}, 
        "$expr": { "$lte": [ { "$strLenCP": "$name" }, 12 ]}}):
        if cyberway_accounts.find({"name":doc["name"]}).count():
            continue

        if id_count - 1 - default_id_count == int(argv):
            break

        account = {
            "id" : bson.Int64(id_count),
            "name" : doc["name"],
            "vm_type" : bson.Int64(0),
            "vm_version" : bson.Int64(0),
            "privileged" : False,
            "last_code_update" : "1970-01-01T00:00:00.000",
            "code_version" : "0000000000000000000000000000000000000000000000000000000000000000",
            "creation_date" : doc["created"].isoformat(),
            "code" : "",
            "abi" : "",
            "_SCOPE_" : "",
            "_PAYER_" : "",
            "_SIZE_" : bson.Int64(65)
        }
        dbs.cyberway_db['account'].save(account)
        if int(argv):
            accounts_list.append(doc["name"])

        accountseq = {
            "id" : bson.Int64(id_count),
            "name" : doc["name"],
            "recv_sequence" : bson.Int64(0),
            "auth_sequence" : bson.Int64(0),
            "code_sequence" : bson.Int64(0),
            "abi_sequence" : bson.Int64(0),
            "_SCOPE_" : "",
            "_PAYER_" : "",
            "_SIZE_" : bson.Int64(48)
        }
        dbs.cyberway_db['accountseq'].save(accountseq)

        reslimit = {
            "id" : bson.Int64(id_count),
            "owner" : doc["name"],
            "pending" : False,
            "net_weight" : bson.Int64(-1),
            "cpu_weight" : bson.Int64(-1),
            "ram_bytes" : bson.Int64(-1),
            "_SCOPE_" : "",
            "_PAYER_" : "",
            "_SIZE_" : bson.Int64(41)
        }
        dbs.cyberway_db['reslimit'].save(reslimit)

        resusage = {
            "id" : bson.Int64(id_count),
            "owner" : doc["name"],
            "net_usage" : {
    	        "last_ordinal" : bson.Int64(0),
    	        "value_ex" : bson.Int64(0),
    	        "consumed" : bson.Int64(0)
            },
            "cpu_usage" : {
    	        "last_ordinal" : bson.Int64(0),
    	        "value_ex" : bson.Int64(0),
    	        "consumed" : bson.Int64(0)
            },
            "ram_usage" : bson.Int64(2724),
            "_SCOPE_" : "",
            "_PAYER_" : "",
            "_SIZE_" : bson.Int64(64)
        }
        dbs.cyberway_db['resusage'].save(resusage)

        id_count += 1

    if len(accounts_list):
        for account in accounts_list:
            for doc in golos_authority_keys.find({"account":account}):
                save_authority(doc)
    else:
        for doc in golos_authority_keys.find({"account":{"$regex":"^[a-z]*([\.]|[a-z]|[1-5])*[a-o\.]$"}, 
            "$expr": { "$lte": [ { "$strLenCP": "$account" }, 12 ]}}):
            save_authority(doc)

    return True

