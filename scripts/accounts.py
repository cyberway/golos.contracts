import dbs
import bson

def convert_accounts():
    golos_accounts = dbs.golos_db['account_object']
    cyberway_accounts = dbs.cyberway_db['account']

    id_count = cyberway_accounts.find().sort('id', -1).limit(1)[0]['id'] + 1

    for doc in golos_accounts.find():
        if cyberway_accounts.find({"name":doc["name"]}).count():
            continue
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

    return True

