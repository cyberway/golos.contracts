from pymongo import MongoClient

client = MongoClient('localhost', 27017)

golos_db = client['Golos']
cyberway_db = client['Cyberway']

