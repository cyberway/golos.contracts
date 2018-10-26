import dbs
import bson

def convert_authority_keys():
    golos_authority_keys = dbs.golos_db['account_authority_object']
    cyberway_permusage = dbs.cyberway_db['permusage']
    cyberway_permission = dbs.cyberway_db['permission']

    id_count_permusage = cyberway_permusage.find().sort('id', -1).limit(1)[0]['id'] + 1
    id_count_permission = cyberway_permission.find().sort('id', -1).limit(1)[0]['id'] + 1

    for doc in golos_authority_keys.find():
        permusage = {
	    "id" : bson.Int64(id_count_permusage),
	    "last_used" : "",
	    "_SCOPE_" : "_CYBERWAY_",
	    "_PAYER_" : "",
	    "_SIZE_" : bson.Int64(16)
        }
        dbs.cyberway_db['permusage'].save(permusage)

        active_permission = {
	    "id" : bson.Int64(id_count_permission),
	    "usage_id" : permusage["id"],
	    "parent" : bson.Int64(0),
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
        
        owner_permission = {
	    "id" : bson.Int64(id_count_permission),
	    "usage_id" : permusage["id"],
	    "parent" : bson.Int64(0),
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
        
        posting_permission = {
	    "id" : bson.Int64(id_count_permission),
	    "usage_id" : permusage["id"],
	    "parent" : bson.Int64(0),
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

    return True

