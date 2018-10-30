import dbs
import json

def create_tags(metadata_tags):
    tags = []
    for tag in metadata_tags or []:
        tag_obj = {
            "tag_name": tag
        }
        tags.append(tag_obj)

    return tags


def convert_posts():
    golos_posts = dbs.golos_db['comment_object']

    length = golos_posts.count()
    dbs.printProgressBar(0, length, prefix = 'Progress:', suffix = 'Complete', length = 50)

    i = 0 
    for doc in golos_posts.find():
       
        try:

            if (doc["removed"]):
                continue
        
            messagestate = {
                "absshares": doc["abs_rshares"],
                "netshares": doc["net_rshares"],
                "voteshares": doc["vote_rshares"],
                "sumcuratorsw": 0
            }
        
            message = {
                "id": dbs.convert_hash(doc["permlink"]),
                "date": doc["last_update"],
                "parentacc": doc["parent_author"],
                "parent_id": 0,
                "tokenprop": 0,
                "beneficiaries": "",
                "rewardweight": doc["reward_weight"],
                "state": messagestate,
                "childcount": doc["children"],
                "closed": "false",
                "level": "",
                "_SCOPE_": doc["author"],
                "_PAYER_": "gls.publish",
                "_SIZE_": 50
            }
            dbs.cyberway_db['posttable'].save(message)

            tags = []
            #print(type(doc["json_metadata"]))
            if (isinstance(doc["json_metadata"], dict)):
                if ("tags" in doc["json_metadata"]):
                    tags = create_tags(doc["json_metadata"]["tags"])

#            if(isinstance(doc["json_metadata"], str)):
#                if (doc["json_metadata"]):
#                    dict_metadata = json.loads(doc["json_metadata"])                    
#                    if (dict_metadata["tags"]):
#                        tags = create_tags(dict_metadata["tags"])

            content = {
                "id": dbs.convert_hash(doc["permlink"]),
                "headermssg": "",
                "bodymssg": doc["body"],
                "languagemssg": "",
                "tags": tags,
                "jsonmetadata": doc["json_metadata"],
                "_SCOPE_": doc["author"],
                "_PAYER_": doc["author"],
                "_SIZE_": 50
            }
            dbs.cyberway_db['contenttable'].save(content) 
       
            i += 1
            dbs.printProgressBar(i + 1, length, prefix = 'Progress:', suffix = 'Complete', length = 50)
        except Exception:
            print(doc)
    
    return True

