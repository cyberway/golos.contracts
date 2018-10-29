import dbs
import bson
from time import sleep

def convert_posts():
    golos_posts = dbs.golos_db['comment_object']

    length = golos_posts.count()
    dbs.printProgressBar(0, length, prefix = 'Progress:', suffix = 'Complete', length = 50)

    id_count = 0 

    for doc in golos_posts.find():
        messagestate = {
            "absshares": doc["abs_rshares"],
            "netshares": doc["net_rshares"],
            "voteshares": doc["vote_rshares"],
            "sumcuratorsw": 0
        }

        message = {
            "id": id_count,
            "date": doc["last_update"],
            "permlink": doc["permlink"],
            "parentacc": doc["parent_author"],
            "parentprmlnk": "",
            "tokenprop": "",
            "beneficiaries": "",
            "rewardweight": doc["reward_weight"],
            "state": messagestate,
            "childcount": doc["children"],
            "closed": "false",
            "level": "",
            "_SCOPE_": doc["author"],
            "_PAYER_": doc["author"],
            "_SIZE_": 65
        }
        dbs.cyberway_db['posttable'].save(message)

        tags = []
        for tag in doc["json_metadata"]["tags"]:
            tag_obj = {
                "tag_name": tag
            }
            tags.append(tag_obj)


        content = {
            "id": id_count,
            "headermssg": "",
            "bodymssg": doc["body"],
            "languagemssg": "",
            "tags": tags,
            "jsonmetadata": doc["json_metadata"],
            "_SCOPE_": doc["author"],
            "_PAYER_": doc["author"],
            "_SIZE_": 48
        }
        dbs.cyberway_db['contenttable'].save(content)
        
        id_count += 1
        
        sleep(0.1)
        dbs.printProgressBar(id_count + 1, length, prefix = 'Progress:', suffix = 'Complete', length = 50)

    
    return True

