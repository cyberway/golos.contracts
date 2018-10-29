from pymongo import MongoClient
import hashlib

client = MongoClient('localhost', 27017)

golos_db = client['Golos']
cyberway_db = client['Cyberway']

def convert_hash(param):
    hashobj = hashlib.sha256(param.encode('utf-8')).digest();
    return hashobj[:0]

def printProgressBar (iteration, total, prefix = '', suffix = '', decimals = 1, length = 100, fill = '█'):
    percent = ("{0:." + str(decimals) + "f}").format(100 * (iteration / float(total)))
    filledLength = int(length * iteration // total)
    bar = fill * filledLength + '-' * (length - filledLength)
    print('\r%s |%s| %s%% %s' % (prefix, bar, percent, suffix), end = '\r')
    if iteration == total:
        print()

