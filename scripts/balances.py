import dbs
import bson

BALANCE_PRECISION = 3
VESTING_PRECISION = 6

golos_accounts = dbs.golos_db['account_object']

def save_balances(doc):
    cyberway_accounts = dbs.cyberway_eosio_token_db['accounts']
    cyberway_balances = dbs.cyberway_gls_vesting_db['balances']

    if (cyberway_accounts.find({"_SCOPE_":doc["name"]}).count() or
            cyberway_balances.find({"_SCOPE_":doc["name"]}).count()):
        return

    balance = {
	"balance": {
	    "amount": doc["balance_value"],
	    "decs": bson.Int64(BALANCE_PRECISION),
	    "sym": "GLS"
	},
	"_SCOPE_": doc["name"],
	"_PAYER_": doc["name"],
	"_SIZE_": bson.Int64(16)
    }
    dbs.cyberway_eosio_token_db['accounts'].save(balance)

    vesting = {
	"vesting": {
	    "amount": doc["vesting_shares_value"],
	    "decs": bson.Int64(VESTING_PRECISION),
	    "sym": "GLS"
	},
	"delegate_vesting": {
	    "amount": doc["delegated_vesting_shares_value"],
	    "decs": bson.Int64(VESTING_PRECISION),
	    "sym": "GLS"
	},
	"received_vesting": {
	    "amount": doc["received_vesting_shares_value"],
	    "decs": bson.Int64(VESTING_PRECISION),
	    "sym": "GLS"
	},
	"unlocked_limit": {
	    "amount": bson.Int64(0),
	    "decs": bson.Int64(VESTING_PRECISION),
	    "sym": "GLS"
	},
	"_SCOPE_": doc["name"],
	"_PAYER_": doc["name"],
	"_SIZE_": bson.Int64(64)
    }
    dbs.cyberway_gls_vesting_db['balances'].save(vesting)


def convert_balances(accounts_list):
    if len(accounts_list):
        for account in accounts_list:
            for doc in golos_accounts.find({"name":account}):
                save_balances(doc)
    else:
        for doc in golos_accounts.find({"name":{"$regex":"^[a-z]*([\.]|[a-z]|[1-5])*[a-o\.]$",
            "$expr": { "$lte": [ { "$strLenCP": "$name" }, 12]}}}):
            save_balances(doc)

