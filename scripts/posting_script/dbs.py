from pymongo import MongoClient
import hashlib

client = MongoClient('localhost', 27017)

golos_db = client['Golos']
cyberway_db = client['Cyberway']

def convert_hash(param):
    hashobj = hashlib.sha256(param.encode('utf-8')).digest();
    return hashobj[:1]
