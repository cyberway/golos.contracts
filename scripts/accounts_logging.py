#!/usr/bin/env python3

# Logging of accounts that do not correspond to the description of eos.

import dbs

golos_accounts = dbs.golos_db['account_object']
is_correct_acc = False

print('Logging in progress, please waiting: ')
file_obj = open('accounts.log', 'w')
for doc in golos_accounts.find():
    for regex_doc in golos_accounts.find({"name":{"$regex":"^[a-z]*([\.]|[a-z]|[1-5])*[a-o\.]$"}, 
        "$expr": { "$lte": [ { "$strLenCP": "$name" }, 12 ]}}):
        if doc["name"] == regex_doc["name"]:
            is_correct_acc = True
            break
        else:
            is_correct_acc = False
    if is_correct_acc == False:
        file_obj.write(doc["name"] + '\n')
file_obj.close()
print('SUCCESS!')
