import dbs
import bson

def convert_accounts_and_keys(argv = -1):
    golos_accounts = dbs.golos_db['account_object']
    cyberway_accounts = dbs.cyberway_db['account']
    
    default_id_count = cyberway_accounts.find().sort('id', -1).limit(1)[0]['id']
    id_count = default_id_count + 1

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
            "creation_date" : doc["created"],
            "code" : "",
            "abi" : "",
            "_SCOPE_" : "_CYBERWAY_",
            "_PAYER_" : "",
            "_SIZE_" : bson.Int64(65)
        }
        dbs.cyberway_db['account'].save(account)

        accountseq = {
            "id" : bson.Int64(id_count),
            "name" : doc["name"],
            "recv_sequence" : bson.Int64(0),
            "auth_sequence" : bson.Int64(0),
            "code_sequence" : bson.Int64(0),
            "abi_sequence" : bson.Int64(0),
            "_SCOPE_" : "_CYBERWAY_",
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
            "_SCOPE_" : "_CYBERWAY_",
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
            "_SCOPE_" : "_CYBERWAY_",
            "_PAYER_" : "",
            "_SIZE_" : bson.Int64(64)
        }
        dbs.cyberway_db['resusage'].save(resusage)

        id_count += 1


    golos_authority_keys = dbs.golos_db['account_authority_object']
    cyberway_permusage = dbs.cyberway_db['permusage']
    cyberway_permission = dbs.cyberway_db['permission']

    default_id_count_permusage = cyberway_permusage.find().sort('id', -1).limit(1)[0]['id']
    id_count_permusage = default_id_count_permusage + 1
    id_count_permission = cyberway_permission.find().sort('id', -1).limit(1)[0]['id'] + 1
    id_count_parent =cyberway_permission.find().sort('parent', -1).limit(1)[0]['parent'] + 1

    for doc in golos_authority_keys.find({"account":{"$regex":"^[a-z]*([\.]|[a-z]|[1-5])*[a-o\.]$"}, 
        "$expr": { "$lte": [ { "$strLenCP": "$account" }, 12 ]}}):
        if (cyberway_permission.find({"owner":doc["account"], "name":"owner"}).count() or 
                cyberway_permission.find({"owner":doc["account"], "name":"active"}).count() or 
                cyberway_permission.find({"owner":doc["account"], "name":"posting"}).count()):
            continue

        if id_count_permusage - 1 - default_id_count_permusage == int(argv):
            break

        permusage = {
	    "id" : bson.Int64(id_count_permusage),
	    "last_used" : "",
	    "_SCOPE_" : "_CYBERWAY_",
	    "_PAYER_" : "",
	    "_SIZE_" : bson.Int64(16)
        }
        dbs.cyberway_db['permusage'].save(permusage)

        owner_permission = {
	    "id" : bson.Int64(id_count_permission),
	    "usage_id" : permusage["id"],
	    "parent" : bson.Int64(id_count_parent),
	    "owner" : doc["account"],
	    "name" : "owner",
	    "last_updated" : doc["last_owner_update"],
	    "auth" : {
		"threshold" : bson.Int64(1),
		"keys" : [
		    {
			"key" : doc["owner"],
			"weight" : bson.Int64(1)
		    }
		],
		"accounts" : [ ],
		"waits" : [ ]
	    },
	    "_SCOPE_" : "_CYBERWAY_",
	    "_PAYER_" : "",
	    "_SIZE_" : bson.Int64(91)
        }
        dbs.cyberway_db['permission'].save(owner_permission)
        id_count_permission += 1

        active_permission = {
	    "id" : bson.Int64(id_count_permission),
	    "usage_id" : permusage["id"],
	    "parent" : bson.Int64(id_count_parent),
	    "owner" : doc["account"],
	    "name" : "active",
	    "last_updated" : "",
	    "auth" : {
		"threshold" : bson.Int64(1),
		"keys" : [
		    {
			"key" : doc["active"],
			"weight" : bson.Int64(1)
		    }
		],
		"accounts" : [ ],
		"waits" : [ ]
	    },
	    "_SCOPE_" : "_CYBERWAY_",
	    "_PAYER_" : "",
	    "_SIZE_" : bson.Int64(91)
        }
        dbs.cyberway_db['permission'].save(active_permission)
        id_count_permission += 1
        
        posting_permission = {
	    "id" : bson.Int64(id_count_permission),
	    "usage_id" : permusage["id"],
	    "parent" : bson.Int64(id_count_parent),
	    "owner" : doc["account"],
	    "name" : "posting",
	    "last_updated" : "",
	    "auth" : {
		"threshold" : bson.Int64(1),
		"keys" : [
		    {
			"key" : doc["posting"],
			"weight" : bson.Int64(1)
		    }
		],
		"accounts" : [ ],
		"waits" : [ ]
	    },
	    "_SCOPE_" : "_CYBERWAY_",
	    "_PAYER_" : "",
	    "_SIZE_" : bson.Int64(91)
        }
        dbs.cyberway_db['permission'].save(posting_permission)

        id_count_permusage += 1
        id_count_permission += 1
        id_count_parent += 1

    return True

