import dbs

def convert_accounts():
    golos_accounts = dbs.golos_db['account_object']

    id_count = 0

    for doc in golos_accounts.find():
        account = {
            "id" : id_count,
            "name" : doc["name"],
            "vm_type" : 0,
            "vm_version" : 0,
            "privileged" : "false",
            "last_code_update" : "1970-01-01T00:00:00.000",
            "code_version" : "0000000000000000000000000000000000000000000000000000000000000000",
            "creation_date" : doc["created"],
            "code" : "",
            "abi" : "",
            "_SCOPE_" : "_CYBERWAY_",
            "_PAYER_" : "",
            "_SIZE_" : 65
        }
        dbs.cyberway_db['account'].save(account)

        accountseq = {
            "id" : id_count,
            "name" : doc["name"],
            "recv_sequence" : 0,
            "auth_sequence" : 0,
            "code_sequence" : 0,
            "abi_sequence" : 0,
            "_SCOPE_" : "_CYBERWAY_",
            "_PAYER_" : "",
            "_SIZE_" : 48
        }
        dbs.cyberway_db['accountseq'].save(accountseq)

        reslimit = {
            "id" : id_count,
            "owner" : doc["name"],
            "pending" : "false",
            "net_weight" : -1,
            "cpu_weight" : -1,
            "ram_bytes" : -1,
            "_SCOPE_" : "_CYBERWAY_",
            "_PAYER_" : "",
            "_SIZE_" : 41
        }
        dbs.cyberway_db['reslimit'].save(reslimit)

        resusage = {
            "id" : id_count,
            "owner" : doc["name"],
            "net_usage" : {
    	        "last_ordinal" : 0,
    	        "value_ex" : 0,
    	        "consumed" : 0
            },
            "cpu_usage" : {
    	        "last_ordinal" : 0,
    	        "value_ex" : 0,
    	        "consumed" : 0
            },
            "ram_usage" : 2724,
            "_SCOPE_" : "_CYBERWAY_",
            "_PAYER_" : "",
            "_SIZE_" : 64
        }
        dbs.cyberway_db['resusage'].save(resusage)

        id_count += 1

    return True

