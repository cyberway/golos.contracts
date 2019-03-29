from pymongo import MongoClient
import hashlib

from decimal import Decimal
from bson.decimal128 import Decimal128


client = MongoClient('172.17.0.1', 27017)

golos_db = client['Golos']
cyberway_db = client['_CYBERWAY_TEST_']

def convert_hash(param):
    hashobj = hashlib.sha256(param.encode('utf-8')).hexdigest()
    return Decimal128(Decimal(int(hashobj[:16], 16)))

def printProgressBar (iteration, total, prefix = '', suffix = '', decimals = 1, length = 100, fill = '█'):
    percent = ("{0:." + str(decimals) + "f}").format(100 * (iteration / float(total)))
    filledLength = int(length * iteration // total)
    bar = fill * filledLength + '-' * (length - filledLength)
    print('\r%s |%s| %s%% %s' % (prefix, bar, percent, suffix), end = '\r')
    if iteration == total:
        print()

