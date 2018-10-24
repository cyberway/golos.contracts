import dbs

def convert_authority_keys():
    golos_authority_keys = dbs.golos_db['account_authority_object']

    id_count = 0

    for doc in golos_authority_keys.find():
        permusage = {
	    "id" : id_count,
	    "last_used" : "",
	    "_SCOPE_" : "_CYBERWAY_",
	    "_PAYER_" : "",
	    "_SIZE_" : 16
        }
        dbs.cyberway_db['permusage'].save(permusage)

        active_permission = {
	    "id" : id_count,
	    "usage_id" : permusage["id"],
	    "parent" : 0,
	    "owner" : doc["account"],
	    "name" : "active",
	    "last_updated" : "",
	    "auth" : {
		"threshold" : 1,
		"keys" : [
		    {
			"key" : doc["active"],
			"weight" : 1
		    }
		],
		"accounts" : [ ],
		"waits" : [ ]
	    },
	    "_SCOPE_" : "_CYBERWAY_",
	    "_PAYER_" : "",
	    "_SIZE_" : 91
        }
        dbs.cyberway_db['permission'].save(active_permission)
        
        owner_permission = {
	    "id" : id_count,
	    "usage_id" : permusage["id"],
	    "parent" : 0,
	    "owner" : doc["account"],
	    "name" : "owner",
	    "last_updated" : doc["last_owner_update"],
	    "auth" : {
		"threshold" : 1,
		"keys" : [
		    {
			"key" : doc["owner"],
			"weight" : 1
		    }
		],
		"accounts" : [ ],
		"waits" : [ ]
	    },
	    "_SCOPE_" : "_CYBERWAY_",
	    "_PAYER_" : "",
	    "_SIZE_" : 91
        }
        dbs.cyberway_db['permission'].save(owner_permission)
        
        posting_permission = {
	    "id" : id_count,
	    "usage_id" : permusage["id"],
	    "parent" : 0,
	    "owner" : doc["account"],
	    "name" : "posting",
	    "last_updated" : "",
	    "auth" : {
		"threshold" : 1,
		"keys" : [
		    {
			"key" : doc["posting"],
			"weight" : 1
		    }
		],
		"accounts" : [ ],
		"waits" : [ ]
	    },
	    "_SCOPE_" : "_CYBERWAY_",
	    "_PAYER_" : "",
	    "_SIZE_" : 91
        }
        dbs.cyberway_db['permission'].save(posting_permission)

        id_count += 1

    return True

