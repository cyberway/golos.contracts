from pymongo import MongoClient

client = MongoClient('172.17.0.1', 27017)

golos_db = client['Golos']
cyberway_db = client['_CYBERWAY_TEST_']
cyberway_eosio_token = client['_CYBERWAY_eosio-token']
cyberway_gls_vesting_db = client['_CYBERWAY_gls-vesting']
